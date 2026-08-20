#include "network_web.h"

#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "audio_player.h"
#include "pins.h"
#include "storage_manager.h"
#include "status_led.h"
#include "wifi_config.h"

namespace {

constexpr uint16_t HTTP_PORT = 80;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr char PREFERENCES_NAMESPACE[] = "audiobooth";
constexpr char PREFERENCES_SSID_KEY[] = "ssid";
constexpr char PREFERENCES_PASSWORD_KEY[] = "password";
constexpr char UPLOAD_FILENAME_HEADER[] = "X-Filename";

enum class NetworkMode {
    Starting,
    ConnectingStation,
    Station,
    AccessPoint,
};

WebServer server(HTTP_PORT);
NetworkMode networkMode = NetworkMode::Starting;
uint32_t stationDeadline = 0;
bool serverStarted = false;
String configuredStationSsid;
String configuredStationPassword;
char deviceNetworkName[16] = "audiobooth_00";
String currentDirectory = "/";
File uploadFile;
String uploadPath;
String uploadError;
bool uploadRejected = false;

uint8_t readBoothId()
{
    constexpr gpio_num_t BOOTH_ID_PINS[] = {
        PIN_BOOTH_ID0,
        PIN_BOOTH_ID1,
        PIN_BOOTH_ID2,
        PIN_BOOTH_ID3,
    };

    uint8_t boothId = 0;
    for (uint8_t bit = 0; bit < 4; ++bit) {
        pinMode(BOOTH_ID_PINS[bit], INPUT_PULLUP);
        // Open = HIGH = logical 0; jumper to ground = LOW = logical 1.
        if (digitalRead(BOOTH_ID_PINS[bit]) == LOW) {
            boothId |= static_cast<uint8_t>(1U << bit);
        }
    }
    return boothId;
}

void makeDeviceNetworkName(uint8_t boothId)
{
    snprintf(deviceNetworkName, sizeof(deviceNetworkName), "%s%02u",
             WIFI_DEVICE_NAME_PREFIX, static_cast<unsigned>(boothId));
}

bool webAccessAllowed()
{
    // LOW is the lifted/active-session state on this booth hardware.
    return digitalRead(PIN_HEADSET_SWITCH) != LOW;
}

void sendBusy()
{
    server.send(423, "text/plain; charset=utf-8", WEB_BUSY_MESSAGE);
}

String htmlEscape(const String &value)
{
    String escaped;
    escaped.reserve(value.length() + 16);

    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
        case '&': escaped += F("&amp;"); break;
        case '<': escaped += F("&lt;"); break;
        case '>': escaped += F("&gt;"); break;
        case '\"': escaped += F("&quot;"); break;
        case '\'': escaped += F("&#39;"); break;
        default: escaped += value[i]; break;
        }
    }

    return escaped;
}

String urlEncode(const String &value)
{
    constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);

    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(value[i]);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_'
            || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += HEX_DIGITS[c >> 4];
            encoded += HEX_DIGITS[c & 0x0F];
        }
    }

    return encoded;
}

int hexDigitValue(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool urlDecode(const String &value, String &decoded)
{
    decoded = "";
    decoded.reserve(value.length());

    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] != '%') {
            decoded += value[i];
            continue;
        }

        if (i + 2 >= value.length()) {
            return false;
        }
        const int high = hexDigitValue(value[i + 1]);
        const int low = hexDigitValue(value[i + 2]);
        if (high < 0 || low < 0) {
            return false;
        }

        const char decodedCharacter = static_cast<char>((high << 4) | low);
        if (decodedCharacter == '\0') {
            return false;
        }
        decoded += decodedCharacter;
        i += 2;
    }
    return true;
}

bool normalizePath(const String &input, String &output)
{
    // Arduino String is null-terminated, so searching for '\0' also finds
    // the normal terminator and would reject every supplied directory path.
    if (input.indexOf('\\') >= 0 || input.indexOf("..") >= 0) {
        return false;
    }

    output = input;
    output.trim();
    if (output.isEmpty()) {
        output = "/";
    }
    if (!output.startsWith("/")) {
        output = "/" + output;
    }

    while (output.indexOf("//") >= 0) {
        output.replace("//", "/");
    }
    while (output.length() > 1 && output.endsWith("/")) {
        output.remove(output.length() - 1);
    }

    return true;
}

bool validFileName(const String &name)
{
    if (name.isEmpty() || name == "." || name == ".."
        || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
        return false;
    }

    for (size_t i = 0; i < name.length(); ++i) {
        if (static_cast<uint8_t>(name[i]) < 0x20) {
            return false;
        }
    }
    return true;
}

String baseName(const String &path)
{
    const int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
}

String childPath(const String &directory, const String &name)
{
    return directory == "/" ? "/" + name : directory + "/" + name;
}

String parentPath(const String &path)
{
    if (path == "/") {
        return "/";
    }

    const int slash = path.lastIndexOf('/');
    return slash <= 0 ? "/" : path.substring(0, slash);
}

String humanSize(size_t bytes)
{
    if (bytes >= 1024U * 1024U) {
        return String(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) + " MB";
    }
    if (bytes >= 1024U) {
        return String(static_cast<double>(bytes) / 1024.0, 1) + " kB";
    }
    return String(bytes) + " B";
}

bool stationActive()
{
    return networkMode == NetworkMode::Station && WiFi.status() == WL_CONNECTED;
}

void redirectToBrowser()
{
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
}

void loadStationCredentials()
{
    Preferences preferences;
    // Open read/write so NVS creates the namespace on first boot instead of
    // emitting NOT_FOUND for a read-only open.
    if (preferences.begin(PREFERENCES_NAMESPACE, false)) {
        if (preferences.isKey(PREFERENCES_SSID_KEY)) {
            configuredStationSsid = preferences.getString(PREFERENCES_SSID_KEY, "");
            configuredStationPassword = preferences.getString(PREFERENCES_PASSWORD_KEY, "");
        }
        preferences.end();
    }

    // Compile-time values are defaults until credentials have been saved
    // through the web interface.
    if (configuredStationSsid.isEmpty()) {
        configuredStationSsid = WIFI_STA_SSID;
        configuredStationPassword = WIFI_STA_PASSWORD;
    }
}

void handleBrowser()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }

    fs::FS *fileSystem = activeFileSystem();
    if (!storageAvailable() || fileSystem == nullptr) {
        server.send(503, "text/plain", "Storage unavailable");
        return;
    }

    if (server.hasArg("dir")) {
        String requestedDirectory;
        if (!normalizePath(server.arg("dir"), requestedDirectory)) {
            server.send(400, "text/plain", "Invalid directory");
            return;
        }

        File requested = fileSystem->open(requestedDirectory);
        const bool usable = requested && requested.isDirectory();
        requested.close();
        if (!usable) {
            server.send(404, "text/plain", "Directory not found");
            return;
        }
        currentDirectory = requestedDirectory;
    }

    File directory = fileSystem->open(currentDirectory);
    if (!directory || !directory.isDirectory()) {
        currentDirectory = "/";
        directory = fileSystem->open(currentDirectory);
    }
    if (!directory || !directory.isDirectory()) {
        server.send(500, "text/plain", "Cannot open storage root");
        return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    server.sendContent(F(
        "<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>"));
    const String escapedDeviceName = htmlEscape(String(deviceNetworkName));
    server.sendContent(escapedDeviceName + " storage");
    server.sendContent(F("</title><style>"
        "body{font-family:system-ui,sans-serif;max-width:850px;margin:2rem auto;padding:0 1rem;color:#222}"
        "table{width:100%;border-collapse:collapse;margin:1rem 0}th,td{padding:.65rem;border-bottom:1px solid #ddd;text-align:left}"
        "button,input::file-selector-button{padding:.45rem .8rem}form{margin:0}.meta{color:#666}.danger{color:#a00}"
        "progress{width:100%;max-width:32rem;margin-top:.7rem}"
        "</style></head><body><h1>"));
    server.sendContent(escapedDeviceName + " storage</h1>");

    String info = "<p class=meta>Device: " + escapedDeviceName
        + " &middot; Storage: " + htmlEscape(storageSourceName());
    info += stationActive() ? " &middot; Wi-Fi client" : " &middot; access point";
    info += "</p><h2>" + htmlEscape(currentDirectory) + "</h2>";
    server.sendContent(info);

    if (currentDirectory != "/") {
        server.sendContent("<p><a href='/?dir=" + urlEncode(parentPath(currentDirectory))
            + "'>&larr; Parent directory</a></p>");
    }

    server.sendContent(F("<table><thead><tr><th>Name</th><th>Size</th><th>Action</th></tr></thead><tbody>"));
    File entry;
    while ((entry = directory.openNextFile())) {
        const String name = baseName(entry.name());
        if (!validFileName(name) || name.equalsIgnoreCase("System Volume Information")) {
            entry.close();
            continue;
        }

        const String escapedName = htmlEscape(name);
        if (entry.isDirectory()) {
            const String target = childPath(currentDirectory, name);
            server.sendContent("<tr><td>&#128193; <a href='/?dir=" + urlEncode(target)
                + "'>" + escapedName + "</a></td><td>&mdash;</td><td></td></tr>");
        } else {
            String fileCell = "&#128196; " + escapedName;
            if (stationActive()) {
                fileCell = "&#128196; <a href='/download?name=" + urlEncode(name)
                    + "'>" + escapedName + "</a>";
            }

            String row = "<tr><td>" + fileCell + "</td><td>" + humanSize(entry.size())
                + "</td><td><form method=post action=/delete>"
                  "<input type=hidden name=name value=\"" + escapedName + "\">"
                  "<button class=danger type=submit onclick=\"return confirm('Delete this file?')\">Delete</button>"
                  "</form></td></tr>";
            server.sendContent(row);
        }
        entry.close();
    }
    directory.close();

    server.sendContent(F(
        "</tbody></table><h2>Upload to this directory</h2>"
        "<form id=uploadForm><input id=uploadFile type=file required> "
        "<button id=uploadButton type=submit>Upload</button></form>"
        "<progress id=uploadProgress value=0 max=100 hidden></progress>"
        "<p id=uploadStatus class=meta>An existing file with the same name is always overwritten.</p>"
        "<script>document.getElementById('uploadForm').addEventListener('submit',function(e){"
        "e.preventDefault();const f=document.getElementById('uploadFile').files[0];if(!f)return;"
        "const b=document.getElementById('uploadButton'),p=document.getElementById('uploadProgress'),s=document.getElementById('uploadStatus');"
        "b.disabled=true;p.hidden=false;p.value=0;s.textContent='Uploading '+f.name+'...';"
        "const x=new XMLHttpRequest();x.open('POST','/upload');"
        "x.setRequestHeader('Content-Type','application/octet-stream');"
        "x.setRequestHeader('X-Filename',encodeURIComponent(f.name));"
        "x.upload.onprogress=function(v){if(v.lengthComputable)p.value=Math.round(v.loaded*100/v.total);};"
        "x.onload=function(){if(x.status>=200&&x.status<300){p.value=100;s.textContent='Upload complete.';location.reload();}"
        "else{b.disabled=false;s.textContent='Upload failed: '+x.responseText;}};"
        "x.onerror=function(){b.disabled=false;s.textContent='Upload connection failed.';};x.send(f);});</script>"
        "<h2>Create directory</h2>"
        "<form method=post action=/mkdir>"
        "<input name=name maxlength=96 placeholder='Directory name' required> "
        "<button type=submit>Create</button></form>"));

    String volumeForm = "<h2>Playback volume</h2>"
        "<p class=meta>The saved volume is used for all audio and remains set after reset or power-off.</p>"
        "<form method=post action=/volume>"
        "<label>Volume <input type=range name=volume min=0 max=100 step=5 value="
        + String(audioVolumePercent())
        + " oninput=\"volumeValue.value=this.value+'%'\"></label> "
          "<output id=volumeValue>" + String(audioVolumePercent()) + "%</output> "
          "<button type=submit>Save volume</button></form>";
    server.sendContent(volumeForm);

    server.sendContent(F(
        "<h2>Wi-Fi for next reset</h2>"
        "<p class=meta>Saving these values does not interrupt the current connection. "
        "They are used after the next reset.</p>"));

    String wifiForm = "<form method=post action=/wifi>"
        "<label>SSID <input name=ssid maxlength=32 required value=\""
        + htmlEscape(configuredStationSsid)
        + "\"></label> <label>Password <input type=password name=password maxlength=63 "
          "placeholder='Leave empty for an open network'></label> "
          "<button type=submit>Save Wi-Fi</button></form>"
        "</body></html>";
    server.sendContent(wifiForm);
    server.sendContent("");
}

void handleCreateDirectory()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }
    if (!server.hasArg("name") || !validFileName(server.arg("name"))
        || server.arg("name").length() > 96) {
        server.send(400, "text/plain", "Invalid directory name");
        return;
    }

    fs::FS *fileSystem = activeFileSystem();
    if (!storageAvailable() || fileSystem == nullptr) {
        server.send(503, "text/plain", "Storage unavailable");
        return;
    }

    const String path = childPath(currentDirectory, server.arg("name"));
    if (fileSystem->exists(path)) {
        server.send(409, "text/plain", "A file or directory with this name already exists");
        return;
    }
    if (!fileSystem->mkdir(path)) {
        server.send(500, "text/plain", "Could not create directory");
        return;
    }

    Serial.printf("Web mkdir: %s\n", path.c_str());
    redirectToBrowser();
}

void handleSaveWifi()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }
    if (!server.hasArg("ssid") || !server.hasArg("password")) {
        server.send(400, "text/plain", "SSID and password fields are required");
        return;
    }

    const String ssid = server.arg("ssid");
    const String password = server.arg("password");
    if (ssid.isEmpty() || ssid.length() > 32) {
        server.send(400, "text/plain", "SSID must contain 1 to 32 characters");
        return;
    }
    if (!password.isEmpty() && (password.length() < 8 || password.length() > 63)) {
        server.send(400, "text/plain", "Password must be empty or contain 8 to 63 characters");
        return;
    }

    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        server.send(500, "text/plain", "Could not open Wi-Fi settings storage");
        return;
    }
    preferences.putString(PREFERENCES_SSID_KEY, ssid);
    preferences.putString(PREFERENCES_PASSWORD_KEY, password);
    preferences.end();

    configuredStationSsid = ssid;
    configuredStationPassword = password;
    Serial.printf("Wi-Fi settings saved for next reset: %s\n", ssid.c_str());

    server.send(200, "text/html; charset=utf-8",
        "<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Wi-Fi saved</title></head><body><h1>Wi-Fi settings saved</h1>"
        "<p>The new SSID and password will be used after the next reset.</p>"
        "<p><a href='/'>Return to storage</a></p></body></html>");
}

void handleSaveVolume()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }
    if (!server.hasArg("volume")) {
        server.send(400, "text/plain", "Volume is required");
        return;
    }

    String value = server.arg("volume");
    value.trim();
    if (value.isEmpty()) {
        server.send(400, "text/plain", "Invalid volume");
        return;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] < '0' || value[i] > '9') {
            server.send(400, "text/plain", "Invalid volume");
            return;
        }
    }

    const long percent = value.toInt();
    if (percent < 0 || percent > 100) {
        server.send(400, "text/plain", "Volume must be between 0 and 100");
        return;
    }
    if (!setAudioVolumePercent(static_cast<uint8_t>(percent))) {
        server.send(500, "text/plain", "Could not save volume");
        return;
    }

    Serial.printf("Playback volume saved: %ld%%\n", percent);
    redirectToBrowser();
}

void handleDelete()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }

    if (!server.hasArg("name") || !validFileName(server.arg("name"))) {
        server.send(400, "text/plain", "Invalid filename");
        return;
    }

    fs::FS *fileSystem = activeFileSystem();
    const String path = childPath(currentDirectory, server.arg("name"));
    File target = fileSystem != nullptr ? fileSystem->open(path) : File();
    const bool isFile = target && !target.isDirectory();
    target.close();

    if (!isFile) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    if (!fileSystem->remove(path)) {
        server.send(500, "text/plain", "Delete failed");
        return;
    }

    Serial.printf("Web delete: %s\n", path.c_str());
    refreshStatusLed();
    redirectToBrowser();
}

void handleDownload()
{
    if (!webAccessAllowed()) {
        sendBusy();
        return;
    }
    if (!stationActive()) {
        server.send(403, "text/plain", "Download is only available on the configured Wi-Fi network");
        return;
    }
    if (!server.hasArg("name") || !validFileName(server.arg("name"))) {
        server.send(400, "text/plain", "Invalid filename");
        return;
    }

    fs::FS *fileSystem = activeFileSystem();
    const String name = server.arg("name");
    const String path = childPath(currentDirectory, name);
    File file = fileSystem != nullptr ? fileSystem->open(path, FILE_READ) : File();
    if (!file || file.isDirectory()) {
        file.close();
        server.send(404, "text/plain", "File not found");
        return;
    }

    server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    server.streamFile(file, "application/octet-stream");
    file.close();
}

void closeFailedUpload()
{
    if (uploadFile) {
        uploadFile.close();
    }
    if (!uploadPath.isEmpty()) {
        fs::FS *fileSystem = activeFileSystem();
        if (fileSystem != nullptr) {
            fileSystem->remove(uploadPath);
        }
    }
}

void handleUploadData()
{
    HTTPRaw &upload = server.raw();

    if (!webAccessAllowed()) {
        uploadRejected = true;
        uploadError = WEB_BUSY_MESSAGE;
        closeFailedUpload();
        server.client().stop();
        return;
    }

    fs::FS *fileSystem = activeFileSystem();
    if (!storageAvailable() || fileSystem == nullptr) {
        uploadRejected = true;
        uploadError = "Storage unavailable";
        closeFailedUpload();
        return;
    }

    if (upload.status == RAW_START) {
        uploadRejected = false;
        uploadError = "";
        uploadPath = "";

        if (!server.hasHeader(UPLOAD_FILENAME_HEADER)) {
            uploadRejected = true;
            uploadError = "Missing filename";
            return;
        }

        String name;
        if (!urlDecode(server.header(UPLOAD_FILENAME_HEADER), name)) {
            uploadRejected = true;
            uploadError = "Invalid filename encoding";
            return;
        }
        name = baseName(name);
        if (!validFileName(name)) {
            uploadRejected = true;
            uploadError = "Invalid filename";
            return;
        }

        uploadPath = childPath(currentDirectory, name);
        fileSystem->remove(uploadPath); // Uploads always overwrite.
        uploadFile = fileSystem->open(uploadPath, FILE_WRITE);
        if (!uploadFile) {
            uploadRejected = true;
            uploadError = "Cannot create file";
        }
        return;
    }

    if (uploadRejected) {
        return;
    }

    if (upload.status == RAW_WRITE) {
        if (!uploadFile || uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
            uploadRejected = true;
            uploadError = "Write failed";
            closeFailedUpload();
            setStatusHardwareError();
            server.client().stop();
        }
    } else if (upload.status == RAW_END) {
        if (uploadFile) {
            uploadFile.close();
        }
        Serial.printf("Web upload: %s (%u bytes)\n", uploadPath.c_str(), upload.totalSize);
        refreshStatusLed();
    } else if (upload.status == RAW_ABORTED) {
        uploadRejected = true;
        uploadError = "Upload aborted";
        closeFailedUpload();
    }
}

void handleUploadFinished()
{
    if (!webAccessAllowed()) {
        closeFailedUpload();
        sendBusy();
        return;
    }
    if (uploadRejected) {
        server.send(500, "text/plain", uploadError);
        return;
    }
    server.send(200, "text/plain", "Upload complete");
}

void registerRoutes()
{
    server.on("/", HTTP_GET, handleBrowser);
    server.on("/mkdir", HTTP_POST, handleCreateDirectory);
    server.on("/wifi", HTTP_POST, handleSaveWifi);
    server.on("/volume", HTTP_POST, handleSaveVolume);
    server.on("/delete", HTTP_POST, handleDelete);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/upload", HTTP_POST, handleUploadFinished, handleUploadData);
    server.onNotFound([]() {
        if (!webAccessAllowed()) {
            sendBusy();
        } else {
            server.send(404, "text/plain", "Not found");
        }
    });
}

void startWebServer()
{
    if (serverStarted) {
        return;
    }

    // NetworkServer::begin() requires an initialized lwIP interface. Call
    // this only after STA connected or softAP() completed successfully.
    server.begin();
    serverStarted = true;
}

void startAccessPoint()
{
    // Do not call disconnect(true) before Wi-Fi has been initialized. It
    // tears down the network stack asynchronously and can invalidate the
    // socket server that is created immediately afterwards.
    WiFi.mode(WIFI_AP);
    WiFi.softAPsetHostname(deviceNetworkName);
    if (!WiFi.softAP(deviceNetworkName, WIFI_AP_PASSWORD)) {
        Serial.println("Could not start fallback access point.");
        return;
    }

    networkMode = NetworkMode::AccessPoint;
    startWebServer();
    Serial.printf("Wi-Fi AP: %s\n", deviceNetworkName);
    Serial.printf("Web interface: http://%s/\n", WiFi.softAPIP().toString().c_str());
}

} // namespace

void beginNetworkAndWeb()
{
    const char *collectedHeaders[] = {UPLOAD_FILENAME_HEADER};
    server.collectHeaders(collectedHeaders, 1);
    registerRoutes();
    loadStationCredentials();

    const uint8_t boothId = readBoothId();
    makeDeviceNetworkName(boothId);
    Serial.printf("Booth ID: %u; network name: %s\n",
                  static_cast<unsigned>(boothId), deviceNetworkName);

    if (configuredStationSsid.isEmpty()) {
        startAccessPoint();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(deviceNetworkName);
    WiFi.setAutoReconnect(true);
    WiFi.begin(configuredStationSsid.c_str(), configuredStationPassword.c_str());
    stationDeadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    networkMode = NetworkMode::ConnectingStation;
    Serial.printf("Connecting to Wi-Fi: %s\n", configuredStationSsid.c_str());
}

void serviceNetworkAndWeb()
{
    if (networkMode == NetworkMode::ConnectingStation) {
        if (WiFi.status() == WL_CONNECTED) {
            networkMode = NetworkMode::Station;
            startWebServer();
            Serial.printf("Wi-Fi connected: %s\n", WiFi.SSID().c_str());
            Serial.printf("Web interface: http://%s/\n", WiFi.localIP().toString().c_str());
        } else if (static_cast<int32_t>(millis() - stationDeadline) >= 0) {
            Serial.println("Configured Wi-Fi unavailable; starting fallback access point.");
            startAccessPoint();
        }
    }

    // The server remains connected during playback. Every handler checks the
    // physical handset state before reading or writing storage.
    if (serverStarted) {
        server.handleClient();
    }
}

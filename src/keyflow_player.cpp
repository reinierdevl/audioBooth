#include "keyflow_player.h"

#include <Arduino.h>
#include <FS.h>

#include "audio_player.h"
#include "keypad.h"
#include "storage_manager.h"

namespace {

constexpr size_t KEY_COUNT = 16;
constexpr char KEYS[KEY_COUNT + 1] = "0123456789*#ABCD";
constexpr char KEYFLOW_FILENAME[] = "keyflow.txt";
constexpr char MAIN_FILENAME[] = "main.mp3";
constexpr char ERROR_FILENAME[] = "error.mp3";
constexpr uint32_t DEFAULT_NO_KEY_SECONDS = 0;

enum class NoKeyAction {
    Silence,
    Next,
    Repeat,
};

enum class PlaybackPurpose {
    Normal,
    ErrorMessage,
};

struct DirectoryConfig {
    String initialFile;
    String keyTargets[KEY_COUNT];
    NoKeyAction noKeyAction = NoKeyAction::Silence;
    uint32_t noKeySeconds = DEFAULT_NO_KEY_SECONDS;
};

String activeDirectory = "/";
DirectoryConfig activeConfig;
String lastPlayedFile;
bool sessionActive = false;
bool waitingForNoKey = false;
uint32_t noKeyDeadline = 0;
PlaybackPurpose playbackPurpose = PlaybackPurpose::Normal;
bool recovering = false;

int keyIndex(char key)
{
    const char normalized = key >= 'a' && key <= 'd' ? key - ('a' - 'A') : key;
    const char *position = strchr(KEYS, normalized);
    return position == nullptr ? -1 : static_cast<int>(position - KEYS);
}

String joinPath(const String &directory, const String &name)
{
    return directory == "/" ? "/" + name : directory + "/" + name;
}

String parentPath(const String &path)
{
    if (path == "/") return "/";
    const int slash = path.lastIndexOf('/');
    return slash <= 0 ? "/" : path.substring(0, slash);
}

bool normalizePath(const String &base, const String &input, String &output)
{
    String candidate = input;
    candidate.trim();
    if (candidate.isEmpty() || candidate.indexOf('\\') >= 0) return false;

    candidate = candidate.startsWith("/") ? candidate : joinPath(base, candidate);
    while (candidate.indexOf("//") >= 0) candidate.replace("//", "/");

    String result = "/";
    int start = 1;
    while (start <= candidate.length()) {
        int slash = candidate.indexOf('/', start);
        if (slash < 0) slash = candidate.length();
        String part = candidate.substring(start, slash);
        if (part == "..") {
            result = parentPath(result);
        } else if (!part.isEmpty() && part != ".") {
            result = joinPath(result, part);
        }
        start = slash + 1;
    }

    output = result;
    return true;
}

bool isMp3(const String &value)
{
    String lower = value;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

bool directoryExists(const String &path)
{
    fs::FS *fs = activeFileSystem();
    if (fs == nullptr) return false;
    File directory = fs->open(path);
    const bool valid = directory && directory.isDirectory();
    directory.close();
    return valid;
}

bool directoryIsEmpty(const String &path)
{
    fs::FS *fs = activeFileSystem();
    if (fs == nullptr) return true;
    File directory = fs->open(path);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        return true;
    }
    File entry = directory.openNextFile();
    const bool empty = !entry;
    entry.close();
    directory.close();
    return empty;
}

void parseNoKey(const String &value, DirectoryConfig &config)
{
    String action = value;
    String seconds;
    const int comma = action.indexOf(',');
    if (comma >= 0) {
        seconds = action.substring(comma + 1);
        action = action.substring(0, comma);
    }
    action.trim();
    action.toLowerCase();
    seconds.trim();

    config.noKeySeconds = seconds.isEmpty() ? DEFAULT_NO_KEY_SECONDS
                                             : static_cast<uint32_t>(seconds.toInt());
    if (action == "next") config.noKeyAction = NoKeyAction::Next;
    else if (action == "repeat") config.noKeyAction = NoKeyAction::Repeat;
    else config.noKeyAction = NoKeyAction::Silence;
}

DirectoryConfig readDirectoryConfig(const String &directory, bool applyRootKeypad)
{
    DirectoryConfig config;
    config.initialFile = MAIN_FILENAME;

    fs::FS *fs = activeFileSystem();
    if (fs == nullptr) return config;
    File file = fs->open(joinPath(directory, KEYFLOW_FILENAME), FILE_READ);
    if (!file || file.isDirectory()) {
        file.close();
        return config;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.isEmpty() || line.startsWith(";")) continue;

        const int equals = line.indexOf('=');
        if (equals < 0) continue;
        String name = line.substring(0, equals);
        String value = line.substring(equals + 1);
        name.trim();
        value.trim();
        String lowerName = name;
        lowerName.toLowerCase();

        if (lowerName == "ini") {
            if (!value.isEmpty()) config.initialFile = value;
        } else if (lowerName == "no_key") {
            parseNoKey(value, config);
        } else if (applyRootKeypad && lowerName == "keypad") {
            setKeypadLayout(value.toInt() == 1 ? 1 : 2);
        } else if (name.length() == 1) {
            const int index = keyIndex(name[0]);
            if (index >= 0) config.keyTargets[index] = value;
        }
    }
    file.close();
    return config;
}

bool resolveFileTarget(const String &value, String &path)
{
    String target = value;
    target.trim();

    // Also accept the documented brace form: {/directory/}filename.mp3.
    if (target.startsWith("{")) {
        const int closing = target.indexOf('}');
        if (closing <= 1 || closing == target.length() - 1) return false;
        String directoryPart = target.substring(1, closing);
        String filePart = target.substring(closing + 1);
        String directory;
        if (!normalizePath(activeDirectory, directoryPart, directory)) return false;
        return normalizePath(directory, filePart, path);
    }
    return normalizePath(activeDirectory, target, path);
}

bool startNormalFile(const String &path)
{
    playbackPurpose = PlaybackPurpose::Normal;
    waitingForNoKey = false;
    if (!playAudioFile(path.c_str())) return false;
    lastPlayedFile = path;
    Serial.printf("Keyflow play: %s\n", path.c_str());
    return true;
}

bool playInitial(bool reportMissing);

void startErrorRecovery(const String &reason)
{
    Serial.printf("Keyflow error: %s\n", reason.c_str());
    stopAudioPlayback();
    waitingForNoKey = false;
    recovering = true;

    fs::FS *fs = activeFileSystem();
    String searchDirectory = activeDirectory;
    while (fs != nullptr) {
        const String errorPath = joinPath(searchDirectory, ERROR_FILENAME);
        if (fs->exists(errorPath)) {
            playbackPurpose = PlaybackPurpose::ErrorMessage;
            if (playAudioFile(errorPath.c_str())) {
                Serial.printf("Keyflow error audio: %s\n", errorPath.c_str());
                return;
            }
        }
        if (searchDirectory == "/") break;
        searchDirectory = parentPath(searchDirectory);
    }

    recovering = false;
    playInitial(false);
}

bool playInitial(bool reportMissing)
{
    String path;
    if (!normalizePath(activeDirectory, activeConfig.initialFile, path)) {
        if (reportMissing) startErrorRecovery("invalid ini/main path");
        return false;
    }

    fs::FS *fs = activeFileSystem();
    if (fs == nullptr || !fs->exists(path)) {
        if (reportMissing && !directoryIsEmpty(activeDirectory)) {
            startErrorRecovery("initial file not found: " + path);
        }
        return false;
    }
    recovering = false;
    return startNormalFile(path);
}

bool activateDirectory(const String &path)
{
    if (!directoryExists(path)) {
        startErrorRecovery("directory not found: " + path);
        return false;
    }
    if (directoryIsEmpty(path)) {
        startErrorRecovery("directory is empty: " + path);
        return false;
    }

    activeDirectory = path;
    activeConfig = readDirectoryConfig(activeDirectory, activeDirectory == "/");
    Serial.printf("Keyflow directory: %s\n", activeDirectory.c_str());
    playInitial(true);
    return true;
}

int compareIgnoreCase(const String &left, const String &right)
{
    String a = left;
    String b = right;
    a.toLowerCase();
    b.toLowerCase();
    return a.compareTo(b);
}

void playNextAlphabetical()
{
    fs::FS *fs = activeFileSystem();
    if (fs == nullptr) return;
    File directory = fs->open(activeDirectory);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        startErrorRecovery("active directory unavailable");
        return;
    }

    String first;
    String next;
    File entry;
    while ((entry = directory.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            String lower = name;
            lower.toLowerCase();
            if (isMp3(name) && lower != ERROR_FILENAME) {
                const String path = joinPath(activeDirectory, name);
                if (first.isEmpty() || compareIgnoreCase(path, first) < 0) first = path;
                if (compareIgnoreCase(path, lastPlayedFile) > 0
                    && (next.isEmpty() || compareIgnoreCase(path, next) < 0)) {
                    next = path;
                }
            }
        }
        entry.close();
    }
    directory.close();

    const String selected = next.isEmpty() ? first : next;
    if (!selected.isEmpty() && !startNormalFile(selected)) {
        startErrorRecovery("could not play next file: " + selected);
    }
}

} // namespace

void beginKeyflowSession()
{
    sessionActive = true;
    activeDirectory = "/";
    lastPlayedFile = "";
    recovering = false;
    waitingForNoKey = false;
    setKeypadLayout(2);
    activeConfig = readDirectoryConfig(activeDirectory, true);
    Serial.println("Keyflow session started at root.");
    playInitial(true);
}

void endKeyflowSession()
{
    sessionActive = false;
    waitingForNoKey = false;
    recovering = false;
    stopAudioPlayback();
    activeDirectory = "/";
    activeConfig = DirectoryConfig();
}

void handleKeyflowKey(char key)
{
    if (!sessionActive) return;
    const int index = keyIndex(key);
    if (index < 0) return;

    // Reload the active directory configuration before resolving the key.
    // Web access is disabled while a session is active, so this cannot race
    // an upload. It also makes configuration changes made while the handset
    // was down unambiguous and avoids acting on an older cached mapping.
    activeConfig = readDirectoryConfig(activeDirectory, activeDirectory == "/");

    // Any key during the no-key waiting interval cancels that pending action.
    waitingForNoKey = false;
    String target = activeConfig.keyTargets[index];
    target.trim();
    if (target.isEmpty()) {
        Serial.printf("Keyflow: directory %s, key %c is not mapped; playback unchanged.\n",
                      activeDirectory.c_str(), key);
        return;
    }

    Serial.printf("Keyflow: directory %s, key %c -> %s\n",
                  activeDirectory.c_str(), key, target.c_str());

    if (isMp3(target) || target.startsWith("{")) {
        String path;
        if (!resolveFileTarget(target, path)) {
            startErrorRecovery("invalid file target for key " + String(key));
            return;
        }
        fs::FS *fs = activeFileSystem();
        if (fs == nullptr || !fs->exists(path) || !startNormalFile(path)) {
            startErrorRecovery("file not found for key " + String(key) + ": " + path);
        }
        return;
    }

    String directory;
    if (!normalizePath(activeDirectory, target, directory)) {
        startErrorRecovery("invalid directory target for key " + String(key));
        return;
    }
    activateDirectory(directory);
}

void serviceKeyflowPlayer()
{
    if (!sessionActive) return;

    if (takeAudioEndEvent()) {
        if (playbackPurpose == PlaybackPurpose::ErrorMessage && recovering) {
            recovering = false;
            playInitial(false);
            return;
        }

        if (activeConfig.noKeyAction == NoKeyAction::Silence) {
            waitingForNoKey = false;
            return;
        }
        waitingForNoKey = true;
        noKeyDeadline = millis() + activeConfig.noKeySeconds * 1000UL;
    }

    if (!waitingForNoKey || static_cast<int32_t>(millis() - noKeyDeadline) < 0) return;
    waitingForNoKey = false;

    if (activeConfig.noKeyAction == NoKeyAction::Repeat) {
        if (!lastPlayedFile.isEmpty() && !startNormalFile(lastPlayedFile)) {
            startErrorRecovery("could not repeat: " + lastPlayedFile);
        }
    } else if (activeConfig.noKeyAction == NoKeyAction::Next) {
        playNextAlphabetical();
    }
}

const char *activeKeyflowDirectory()
{
    return activeDirectory.c_str();
}

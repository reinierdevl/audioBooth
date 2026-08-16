#include "status_led.h"

#include <Arduino.h>
#include <FS.h>

#include "pins.h"
#include "storage_manager.h"

namespace {

constexpr uint8_t LED_BRIGHTNESS = 24;
constexpr uint8_t MAX_DIRECTORY_DEPTH = 8;
bool hardwareError = false;

bool isMp3File(const String &name)
{
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

bool containsMp3(const String &directoryPath, uint8_t depth)
{
    fs::FS *fileSystem = activeFileSystem();
    if (fileSystem == nullptr || depth > MAX_DIRECTORY_DEPTH) {
        return false;
    }

    File directory = fileSystem->open(directoryPath);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        return false;
    }

    File entry;
    while ((entry = directory.openNextFile())) {
        String name = entry.name();
        const int slash = name.lastIndexOf('/');
        const String baseName = slash >= 0 ? name.substring(slash + 1) : name;

        if (entry.isDirectory()) {
            const bool hiddenSystemDirectory =
                baseName.equalsIgnoreCase("System Volume Information");
            entry.close();

            if (!hiddenSystemDirectory) {
                const String childPath = directoryPath == "/"
                    ? "/" + baseName
                    : directoryPath + "/" + baseName;
                if (containsMp3(childPath, depth + 1)) {
                    directory.close();
                    return true;
                }
            }
        } else {
            const bool found = isMp3File(baseName);
            entry.close();
            if (found) {
                directory.close();
                return true;
            }
        }
    }

    directory.close();
    return false;
}

void setColor(uint8_t red, uint8_t green, uint8_t blue, const char *name)
{
    rgbLedWrite(PIN_STATUS_LED, red, green, blue);
    Serial.printf("Status LED: %s\n", name);
}

} // namespace

void beginStatusLed(bool hardwareHealthy)
{
    hardwareError = !hardwareHealthy;
    refreshStatusLed();
}

void refreshStatusLed()
{
    if (hardwareError || !storageAvailable()
        || storageSource() == StorageSource::None) {
        setColor(LED_BRIGHTNESS, 0, 0, "red (hardware error)");
        return;
    }

    if (!containsMp3("/", 0)) {
        setColor(LED_BRIGHTNESS, LED_BRIGHTNESS / 4, 0,
            "orange (no MP3 files found)");
        return;
    }

    if (storageSource() == StorageSource::SdCard) {
        setColor(0, LED_BRIGHTNESS, 0, "green (SD card)");
    } else {
        setColor(LED_BRIGHTNESS, LED_BRIGHTNESS, 0,
            "yellow (internal FFat)");
    }
}

void setStatusHardwareError()
{
    hardwareError = true;
    refreshStatusLed();
}

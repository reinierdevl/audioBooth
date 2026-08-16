#include "storage_manager.h"

#include <Arduino.h>
#include <FFat.h>
#include <SD.h>
#include <SPI.h>

#include "pins.h"

namespace {

constexpr uint32_t SD_SPI_FREQUENCY_HZ = 10'000'000;
constexpr uint8_t SD_MOUNT_ATTEMPTS = 3;

SPIClass sdSpi(FSPI);
StorageSource selectedSource = StorageSource::None;
fs::FS *selectedFileSystem = nullptr;
bool selectedStorageAvailable = false;

bool mountSdCard()
{
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    for (uint8_t attempt = 0; attempt < SD_MOUNT_ATTEMPTS; ++attempt) {
        if (SD.begin(PIN_SD_CS, sdSpi, SD_SPI_FREQUENCY_HZ)) {
            if (SD.cardType() != CARD_NONE) {
                return true;
            }

            SD.end();
        }

        delay(100);
    }

    return false;
}

} // namespace

bool beginStorage()
{
    // Storage selection is intentionally performed only once per boot.
    if (selectedSource != StorageSource::None) {
        return selectedStorageAvailable;
    }

    if (mountSdCard()) {
        selectedSource = StorageSource::SdCard;
        selectedFileSystem = &SD;
        selectedStorageAvailable = true;
        return true;
    }

    // Format a new/empty internal FFat partition on its first boot.
    if (FFat.begin(true)) {
        selectedSource = StorageSource::Internal;
        selectedFileSystem = &FFat;
        selectedStorageAvailable = true;
        return true;
    }

    selectedSource = StorageSource::None;
    selectedFileSystem = nullptr;
    selectedStorageAvailable = false;
    return false;
}

StorageSource storageSource()
{
    return selectedSource;
}

fs::FS *activeFileSystem()
{
    return selectedFileSystem;
}

bool storageAvailable()
{
    return selectedStorageAvailable;
}

bool contentLayoutValid()
{
    if (!selectedStorageAvailable || selectedFileSystem == nullptr) {
        return false;
    }

    return selectedFileSystem->exists("/keyflow.txt")
        && selectedFileSystem->exists("/error.mp3");
}

void markStorageUnavailable()
{
    // Do not switch to the other filesystem after startup.
    selectedStorageAvailable = false;
}

const char *storageSourceName()
{
    switch (selectedSource) {
    case StorageSource::Internal:
        return "internal FFat";
    case StorageSource::SdCard:
        return "SD card";
    case StorageSource::None:
    default:
        return "none";
    }
}


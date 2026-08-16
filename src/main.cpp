#include <Arduino.h>

#include "audio_player.h"
#include "keypad.h"
#include "keyflow_player.h"
#include "network_web.h"
#include "pins.h"
#include "storage_manager.h"
#include "status_led.h"

namespace {

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint32_t HANDSET_DEBOUNCE_MS = 30;

bool handsetLifted()
{
    // Verify this polarity on the physical booth hardware.
    return digitalRead(PIN_HEADSET_SWITCH) == LOW;
}

void printBootReport()
{
    Serial.println();
    Serial.println("PhoneBooth universal");
    Serial.println("--------------------");
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("Cores: %u\n", ESP.getChipCores());
    Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
    Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Storage: %s\n", storageSourceName());
    Serial.printf("Content layout: %s\n", contentLayoutValid() ? "valid" : "missing files");
    Serial.printf("Handset: %s\n", handsetLifted() ? "lifted" : "down");
}

void onHandsetLifted()
{
    Serial.println("Handset lifted: starting keyflow session.");

    if (!storageAvailable()) {
        Serial.println("Storage unavailable; playback remains silent until restart.");
        return;
    }

    beginKeyflowSession();
}

void onHandsetDown()
{
    Serial.println("Handset down: stop all player activity and enter standby.");
    endKeyflowSession();
}

} // namespace

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(250);

    pinMode(PIN_HEADSET_SWITCH, INPUT_PULLUP);
    beginKeypad();

    const bool storageReady = beginStorage();
    const bool audioReady = beginAudioPlayer();
    beginStatusLed(storageReady && audioReady);
    printBootReport();
    // Start the asynchronous Wi-Fi stack only after the synchronous hardware
    // report has completed. This avoids overlapping network initialization
    // with ESP flash/PSRAM queries and serial reporting during boot.
    beginNetworkAndWeb();

    if (handsetLifted()) {
        onHandsetLifted();
    }
}

void loop()
{
    static bool stableState = handsetLifted();
    static bool lastReading = stableState;
    static uint32_t changedAt = millis();

    const bool reading = handsetLifted();
    const uint32_t now = millis();

    if (reading != lastReading) {
        lastReading = reading;
        changedAt = now;
    }

    if (reading != stableState && now - changedAt >= HANDSET_DEBOUNCE_MS) {
        stableState = reading;

        if (stableState) {
            onHandsetLifted();
        } else {
            onHandsetDown();
        }
    }

    const char pressedKey = keypadPressEvent();
    if (pressedKey != '\0') {
        Serial.printf("Key pressed: %c\n", pressedKey);

        if (stableState) {
            handleKeyflowKey(pressedKey);
        }
    }

    serviceAudioPlayback();
    serviceKeyflowPlayer();
    serviceNetworkAndWeb();

    vTaskDelay(1);
}

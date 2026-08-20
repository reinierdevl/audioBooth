#include "audio_player.h"

#include <Arduino.h>
#include <Audio.h>
#include <Preferences.h>
#include <Wire.h>

#include "pins.h"
#include "storage_manager.h"

namespace {

constexpr uint8_t ES8311_ADDRESS = 0x18;
// Keep the codec itself capable of full output. The persistent website
// setting controls the decoder level from 0% through the real 100% maximum.
constexpr uint8_t ES8311_PLAYBACK_VOLUME = 100; // 0..100
constexpr uint8_t DEFAULT_VOLUME_PERCENT = 40;
constexpr char AUDIO_PREFERENCES_NAMESPACE[] = "audiobooth";
constexpr char AUDIO_VOLUME_KEY[] = "volume_pct";

Audio audio;
bool codecReady = false;
bool playing = false;
volatile bool endEvent = false;
uint8_t outputVolumePercent = DEFAULT_VOLUME_PERCENT;

void loadStoredOutputVolume()
{
    Preferences preferences;
    if (!preferences.begin(AUDIO_PREFERENCES_NAMESPACE, false)) {
        outputVolumePercent = DEFAULT_VOLUME_PERCENT;
        Serial.println("Could not open volume settings; using 40%.");
        return;
    }

    outputVolumePercent = preferences.getUChar(
        AUDIO_VOLUME_KEY, DEFAULT_VOLUME_PERCENT);
    preferences.end();
    if (outputVolumePercent > 100) {
        outputVolumePercent = DEFAULT_VOLUME_PERCENT;
    }
}

bool writeCodecRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(ES8311_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool setCodecVolume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    const uint8_t registerValue = volume == 0
        ? 0
        : static_cast<uint8_t>((static_cast<uint16_t>(volume) * 256U / 100U) - 1U);
    return writeCodecRegister(0x32, registerValue);
}

bool beginCodecWithoutMclk()
{
    if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000)) {
        return false;
    }

    Wire.beginTransmission(ES8311_ADDRESS);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    bool ok = writeCodecRegister(0x00, 0x1F); // Reset codec blocks.
    delay(20);
    ok = writeCodecRegister(0x00, 0x00) && ok;
    ok = writeCodecRegister(0x00, 0x80) && ok; // Power on, slave mode.

    // Use BCLK as the internal MCLK. Audio output is 16-bit stereo, so BCLK
    // is 32 * sample rate; the x8 pre-multiplier produces the required 256fs.
    ok = writeCodecRegister(0x01, 0x9F) && ok;
    ok = writeCodecRegister(0x02, 0x18) && ok;
    ok = writeCodecRegister(0x03, 0x10) && ok;
    ok = writeCodecRegister(0x04, 0x10) && ok;
    ok = writeCodecRegister(0x05, 0x00) && ok;
    ok = writeCodecRegister(0x06, 0x03) && ok;
    ok = writeCodecRegister(0x07, 0x00) && ok;
    ok = writeCodecRegister(0x08, 0xFF) && ok;

    // Standard I2S, 16-bit input/output.
    ok = writeCodecRegister(0x09, 0x0C) && ok;
    ok = writeCodecRegister(0x0A, 0x0C) && ok;

    ok = writeCodecRegister(0x0D, 0x01) && ok;
    ok = writeCodecRegister(0x0E, 0x02) && ok;
    ok = writeCodecRegister(0x12, 0x00) && ok;
    ok = writeCodecRegister(0x13, 0x10) && ok;
    ok = writeCodecRegister(0x1C, 0x6A) && ok;
    ok = writeCodecRegister(0x37, 0x08) && ok;

    // Configure the hardware gain once. Runtime mute/unmute is done in the
    // decoder so stopping I2S never needs a subsequent I2C transaction.
    ok = setCodecVolume(ES8311_PLAYBACK_VOLUME) && ok;
    return ok;
}

void audioInfo(Audio::msg_t message)
{
    // This callback runs from the library's audio task. The message text
    // pointers are owned by that task and can already be invalid when a
    // concurrent Serial printf formats them. Only copy the event state here;
    // all application logging remains in the Arduino loop task.
    if (message.e == Audio::evt_eof) {
        endEvent = true;
    }
}

} // namespace

bool beginAudioPlayer()
{
    Audio::audio_info_callback = audioInfo;
    loadStoredOutputVolume();
    Serial.printf("Playback volume: %u%%\n",
                  static_cast<unsigned>(outputVolumePercent));

    codecReady = beginCodecWithoutMclk();
    if (!codecReady) {
        Serial.println("ES8311 initialization failed.");
        return false;
    }

    if (!audio.setPinout(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT)) {
        Serial.println("I2S pin configuration failed.");
        codecReady = false;
        return false;
    }

    // Match the decoder's number of volume steps to the website percentage,
    // so every stored value maps directly from 0 through the true 100% level.
    audio.setVolumeSteps(100);

    // Start silent. Lifting the handset enables the decoder volume.
    audio.setVolume(0);
    return true;
}

bool playAudioFile(const char *path)
{
    stopAudioPlayback();
    endEvent = false;

    fs::FS *fileSystem = activeFileSystem();
    if (!codecReady || !storageAvailable() || fileSystem == nullptr) {
        Serial.println("Cannot play audio: codec or storage unavailable.");
        return false;
    }

    if (!fileSystem->exists(path)) {
        Serial.printf("Audio file not found: %s\n", path);
        return false;
    }

    audio.setVolume(outputVolumePercent);
    playing = audio.connecttoFS(*fileSystem, path);
    if (!playing) {
        audio.setVolume(0);
        Serial.printf("Could not open audio file: %s\n", path);
        return false;
    }

    Serial.printf("Playing: %s\n", path);
    return true;
}

void stopAudioPlayback()
{
    endEvent = false;
    // Silence samples before closing the decoder/file. Do not access the
    // ES8311 over I2C here; some Arduino 3.x I2S stop paths can leave the
    // shared peripheral manager in a state that rejects the next transfer.
    audio.setVolume(0);

    if (playing) {
        audio.stopSong();
        playing = false;
        Serial.println("Playback stopped.");
    }
}

void serviceAudioPlayback()
{
    if (playing) {
        audio.loop();
    }
}

bool audioIsPlaying()
{
    return playing;
}

uint8_t audioVolumePercent()
{
    return outputVolumePercent;
}

bool setAudioVolumePercent(uint8_t percent)
{
    if (percent > 100) return false;

    Preferences preferences;
    if (!preferences.begin(AUDIO_PREFERENCES_NAMESPACE, false)) {
        return false;
    }
    const size_t written = preferences.putUChar(AUDIO_VOLUME_KEY, percent);
    preferences.end();
    if (written != sizeof(percent)) {
        return false;
    }

    outputVolumePercent = percent;
    if (playing) {
        audio.setVolume(outputVolumePercent);
    }
    return true;
}

bool takeAudioEndEvent()
{
    if (!endEvent) {
        return false;
    }
    endEvent = false;
    playing = false;
    return true;
}

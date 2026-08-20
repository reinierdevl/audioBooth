# PhoneBooth universal

For simple firmware installation from an Apple MacBook, see
[Installing AudioBooth Firmware from a MacBook](docs/FLASH_FIRMWARE_MACBOOK.md).

For simple firmware installation from a Windows PC, see
[Installing AudioBooth Firmware from Windows](docs/FLASH_FIRMWARE_WINDOWS.md).

Standalone ESP32-S3 N16R8 telephone-handset MP3 player.

## Development environment

- Visual Studio Code
- PlatformIO
- pioarduino platform 55.03.39
- Arduino-ESP32 3.3.9
- Board: `esp32-s3-devkitc1-n16r8`

## First build

Open this directory as a PlatformIO project and run **Build**. Connect the
ESP32-S3 through USB and then run **Upload** and **Monitor**.

At startup, the firmware:

1. tries to mount the optional FAT32 SD card;
2. selects SD for the entire boot when mounting succeeds;
3. otherwise mounts or initializes internal FFat;
4. never changes storage source until reset;
5. reports board, memory, storage, content, and handset state over Serial.

The selected storage should contain at least:

```text
/
|-- keyflow.txt
|-- main.mp3
`-- error.mp3
```

See [AUDIO_GEBRUIK_NL.md](AUDIO_GEBRUIK_NL.md) for the current Dutch audio
and key-flow specification, and [USE_CASES.md](USE_CASES.md) for the complete
functional scope.

## Wi-Fi and storage web interface

Set default values for `WIFI_STA_SSID` and `WIFI_STA_PASSWORD` in
`include/wifi_config.h`, or enter credentials on the web page. Web-entered
credentials are stored in ESP32 NVS and take effect after the next reset. If
the SSID is empty or the first connection attempt does not succeed within 15
seconds, the device starts access point `audiobooth_xx` with password
`0123456789`.

The `xx` suffix is the decimal value `00`–`15` read from the four active-low
booth-ID jumpers (`ID0` is the least-significant bit). The same name is used
as the hostname when the unit joins an existing Wi-Fi network.

The selected network mode remains active when the handset is lifted, but all
web requests are blocked while that playback session is active. With the
handset down, the English-language interface can browse and create
directories, upload/overwrite files, delete files, and save Wi-Fi credentials
for the next reset. File download is available only while connected to the
configured existing Wi-Fi network.

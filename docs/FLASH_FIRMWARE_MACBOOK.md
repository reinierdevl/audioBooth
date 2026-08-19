# Installing AudioBooth Firmware from a MacBook

This guide is intended for someone without programming experience. You do
not need PlatformIO, Arduino IDE, Python or Terminal.

## What you need

- An Apple MacBook with an internet connection.
- Google Chrome or Microsoft Edge version 89 or newer.
- The ESP32-S3 AudioBooth device.
- A USB cable that supports data. Some charging cables do not carry data.
- The supplied file named `firmware.factory.bin`.

> **Important:** Safari cannot use the required Web Serial connection. Use
> Google Chrome or Microsoft Edge.

## Before you start

1. Save `firmware.factory.bin` in an easy-to-find location such as
   **Downloads**.
2. Close PlatformIO, Arduino IDE and any serial-monitor program if they are
   open. Only one program can use the ESP32 USB connection at a time.
3. Connect the AudioBooth device directly to the MacBook with the USB cable.
   Avoid using a USB hub if possible.
4. If macOS asks whether the accessory may connect, select **Allow**.

## Open the Espressif flashing tool

1. Open Google Chrome or Microsoft Edge.
2. Visit the official Espressif tool:

   <https://espressif.github.io/esptool-js/>

3. Find the **Program** section.
4. Set **Baudrate** to `460800`. If programming later fails, retry with
   `115200`.
5. Select **Connect**.
6. The browser displays a list of USB/serial devices. Select the device that
   appeared when the AudioBooth was connected. Its name may include
   `Espressif`, `USB JTAG/serial debug unit`, `CP210x`, `CH340` or `USB
   Serial`.
7. Select **Connect** in the browser dialog.

The tool should report that it has connected to an **ESP32-S3**.

## If the tool cannot connect

Put the ESP32-S3 into download mode:

1. Keep the USB cable connected.
2. Press and hold the button marked **BOOT**.
3. While holding **BOOT**, briefly press and release **RESET** or **EN**.
4. Release **BOOT**.
5. Select **Connect** in the Espressif tool again.

If no serial device appears at all, try another USB cable or USB port. The
most common cause is a charge-only USB cable.

## Select the firmware

In the **Program** section:

1. Enter this value in **Flash Address**:

   ```text
   0x0
   ```

2. Select the file button on the same row.
3. Choose the supplied `firmware.factory.bin` file from **Downloads**.
4. Use these settings if the page asks for them:

   | Setting | Value |
   |---|---|
   | Flash Mode | `DIO` |
   | Flash Frequency | `80m` or `80 MHz` |
   | Flash Size | `16MB` |

The file is a combined factory image. Do not add separate bootloader,
partition or application files.

## Program the ESP32-S3

1. Check once more that the address next to `firmware.factory.bin` is `0x0`.
2. Select **Program**.
3. Do not unplug the USB cable, close the browser or allow the MacBook to
   sleep while programming is in progress.
4. Wait until the tool reports that programming has completed successfully.
5. Select **Disconnect** if that button is available.
6. Press and release **RESET** or **EN** on the AudioBooth. If there is no
   reset button, unplug USB, wait five seconds and reconnect it.

The installation is now complete.

## What happens after installation

The firmware checks for an SD card only during startup:

- With a usable SD card, the status LED becomes green.
- Without an SD card, internal storage is used and the LED becomes yellow.
- An orange LED means that no playable files were found.
- A red LED indicates a hardware error.

The firmware file does not contain the MP3 audio collection. Audio must be
provided on a FAT32 SD card or uploaded to internal storage through the
AudioBooth website.

If AudioBooth cannot join its configured Wi-Fi network, it creates a network
named `audiobooth_xx`, where `xx` is its booth ID. The password is:

```text
0123456789
```

## Troubleshooting

### No device is shown after selecting Connect

- Confirm that Chrome or Edge is being used, not Safari.
- Try another data-capable USB cable.
- Connect directly to the MacBook instead of through a hub.
- Close other applications that may be using the serial port.
- Disconnect and reconnect the USB cable, then reload the page.

### Connection fails after selecting the device

- Follow the **download mode** procedure above.
- Reload the Espressif page and reconnect.
- Try baudrate `115200`.

### Programming stops or reports an error

- Do not disconnect the board.
- Reload the page and repeat the complete connection procedure.
- Use baudrate `115200`.
- Try a shorter or better-quality USB data cable.

### The device does not start after successful programming

1. Disconnect the tool from the serial port.
2. Press **RESET** or **EN** once.
3. If necessary, unplug USB for five seconds and reconnect it without holding
   **BOOT**.

## About Erase Flash

Do not select **Erase Flash** unless the person who supplied the firmware
specifically asks you to do so. Erasing removes saved Wi-Fi settings and all
audio stored in the ESP32 internal storage. It does not affect an SD card.


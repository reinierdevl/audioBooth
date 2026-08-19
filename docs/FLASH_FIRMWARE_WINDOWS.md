# Installing AudioBooth Firmware from a Windows PC

This guide is intended for someone without programming experience. You do
not need PlatformIO, Arduino IDE, Python or Command Prompt.

## What you need

- A Windows PC with an internet connection.
- Google Chrome or Microsoft Edge version 89 or newer.
- The ESP32-S3 AudioBooth device.
- A USB cable that supports data. Some charging cables do not carry data.
- The supplied file named `firmware.factory.bin`.

## Before you start

1. Save `firmware.factory.bin` in an easy-to-find location such as
   **Downloads**.
2. Close PlatformIO, Arduino IDE and any serial-monitor program if they are
   open. Only one program can use the ESP32 USB connection at a time.
3. Connect the AudioBooth directly to the PC with the USB cable. Avoid using
   a USB hub if possible.
4. Wait a few seconds while Windows detects the device and installs its
   driver.

## Open the Espressif flashing tool

1. Open Google Chrome or Microsoft Edge.
2. Visit the official Espressif tool:

   <https://espressif.github.io/esptool-js/>

3. Find the **Program** section.
4. Set **Baudrate** to `460800`. If programming later fails, retry with
   `115200`.
5. Select **Connect**.
6. The browser displays a list of serial devices. Select the device that
   appeared when the AudioBooth was connected. Its name may include
   `Espressif`, `USB JTAG/serial debug unit`, `CP210x`, `CH340` or `USB
   Serial`. Windows may also show a COM-port number such as `COM5`.
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
3. Do not unplug the USB cable, close the browser or allow the PC to sleep
   while programming is in progress.
4. Wait until the tool reports that programming has completed successfully.
5. Select **Disconnect** if that button is available.
6. Press and release **RESET** or **EN** on the AudioBooth. If there is no
   reset button, unplug USB, wait five seconds and reconnect it.

The installation is now complete.

## Alternative: Espressif Flash Download Tool for Windows

Use this method if the browser method is unavailable or if the firmware
supplier specifically asks you to use the Windows application.

### Download the application

1. Open the official Espressif download page:

   <https://www.espressif.com/en/support/download/other-tools>

2. Find **Flash Download Tools** and download the latest Windows version.
3. Open the downloaded ZIP file and extract all its contents to a new folder.
4. Open that folder and start the Flash Download Tool `.exe` file. If Windows
   displays a security question, confirm only if the file was downloaded from
   the official `espressif.com` website.

### Select the device type

In the first window, select:

| Setting | Value |
|---|---|
| ChipType | `ESP32-S3` |
| WorkMode | `Develop` |
| LoadMode | `UART` |

Then select **OK**.

`UART` is also normally used when the serial connection reaches the PC
through a USB cable. If the supplied hardware specifically uses native USB
download mode, the supplier may instruct you to select `USB` instead.

### Select the combined firmware file

1. In the first firmware row, select the file-selection button.
2. Choose the supplied `firmware.factory.bin` file.
3. Enter this address in the address box on the same row:

   ```text
   0x0
   ```

4. Enable the checkbox at the left of that firmware row.
5. Leave all other firmware rows empty and disabled. The factory file already
   contains the bootloader, partition table and application.

### Configure the flash

Use the following settings:

| Setting | Value |
|---|---|
| SPI SPEED | `80MHz` |
| SPI MODE | `DIO` |
| FLASH SIZE | `16MB` |
| BAUD | `460800` |

If programming is unreliable, reduce **BAUD** to `115200`. Do not change the
firmware address from `0x0`.

### Select the COM port and program

1. Connect the AudioBooth directly to the Windows PC with a USB data cable.
2. Select the COM port that appears when the device is connected, for example
   `COM5`.
3. If no COM port appears, follow the driver and cable checks in the
   troubleshooting section below.
4. Select **START**.
5. If the tool remains at `Connecting`, put the ESP32-S3 into download mode:
   - Press and hold **BOOT**.
   - Briefly press and release **RESET** or **EN**.
   - Release **BOOT**.
   - Select **START** again.
6. Wait until the tool displays **FINISH**. Do not disconnect the USB cable
   while the progress bar is moving.
7. Close or stop the Flash Download Tool so that it releases the COM port.
8. Press **RESET** or **EN** once. Alternatively, unplug USB, wait five
   seconds and reconnect it without holding **BOOT**.

The AudioBooth firmware should now start.

### Erasing with the Windows tool

Do not select **ERASE** unless the firmware supplier specifically asks you to
perform a clean installation. Erasing removes saved Wi-Fi settings and all
audio in internal ESP32 storage. After erasing, program
`firmware.factory.bin` again at address `0x0`. An SD card is not erased.

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

- Try another data-capable USB cable.
- Try another USB port and connect directly instead of through a hub.
- Close other applications that may be using the COM port.
- Disconnect and reconnect the USB cable, then reload the page.
- Open **Device Manager** and look under **Ports (COM & LPT)** or **Universal
  Serial Bus devices**. Reconnect the AudioBooth and check which entry
  appears.
- If Windows shows an unknown device or a warning symbol, ask the supplier
  which USB driver is required for the fitted USB-to-serial interface.

### Connection fails after selecting the device

- Follow the **download mode** procedure above.
- Reload the Espressif page and reconnect.
- Try baudrate `115200`.
- Make sure no serial monitor has the same COM port open.

### Programming stops or reports an error

- Do not disconnect the board while the tool is still writing.
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

# AudioBooth User Manual


## LED color codes

| Color | Meaning |
|---|---|
| Green | SD card selected and ready |
| Yellow | Internal FFat storage selected and ready |
| Orange | No playable files found |
| Red | Hardware error |

## Overview

AudioBooth is a standalone telephone-handset MP3 player. It uses a microSD
card or ESP32 internal FFat storage; no external server is required. 

Lifting the handset starts a session in the root directory. Keypad choices can
play files or open menu directories. Replacing the handset stops playback and
resets the next session to root.

The network stays connected during playback, but the website is blocked while
the handset is lifted and reports:

```text
user interface not available, device in use
```

## Storage selection

Storage is selected once during reset or power-up:

1. AudioBooth tries to mount the microSD card.
2. If successful, SD is used for the entire boot.
3. Otherwise, internal FFat is used.

Only the selected storage is visible. If the selected SD card is removed,
playback and file access can fail. Reinserting it does not cause fallback to
FFat and may not restore operation. Reinsert it and reset or power-cycle.

### SD-card requirements

- Recommended capacity: 32 GB or smaller; 16 GB is suitable.
- Partition table: MBR.
- One primary partition.
- Filesystem: FAT32.
- Allocation unit: formatter default.
- Insert the card before reset or power-up.

exFAT, NTFS and multiple partitions are not supported. Cards larger than
32 GB may work when manually formatted as FAT32 but are not recommended for
the first installation. There is no card-detect switch; the firmware checks
the card through SPI.

## Required files

Recommended root layout:

```text
/
|-- main.mp3
|-- error.mp3
`-- keyflow.txt
```

- `main.mp3` is the default initial audio.
- `keyflow.txt` optionally defines another initial file and keypad actions.
- `error.mp3` is the fallback message for invalid targets.

If no playable file exists, AudioBooth remains silent. Match filename spelling
and capitalization exactly.

Recommended format for telephone speech:

```text
Codec:       MP3
Channels:    mono
Sample rate: 22,050 Hz
Bitrate:     32 kbit/s CBR
```

Mono 44.1 kHz at 64 kbit/s also works but requires about twice the storage.
Do not rename another audio format to `.mp3`.

The current partition table provides 12 MiB of internal FFat storage. The
following estimates reserve approximately 5% for filesystem overhead:

| MP3 format | Typical handset quality | Estimated duration |
|---|---|---:|
| Mono, 16 kbit/s | Very low; speech only | ~100 min |
| Mono, 24 kbit/s | Basic speech | ~66 min |
| Mono, 32 kbit/s | Good telephone speech | ~50 min |
| Mono, 40 kbit/s | Clear speech | ~40 min |
| Mono, 48 kbit/s | Very good speech | ~33 min |
| Mono, 56 kbit/s | Speech and simple music | ~28 min |
| Mono, 64 kbit/s | High quality for a handset | ~25 min |
| Mono, 80 kbit/s | More than usually needed | ~20 min |
| Mono, 96 kbit/s | High-quality mono | ~16.5 min |
| Mono, 128 kbit/s | Unnecessary for this handset | ~12.5 min |

## Connecting to the website

If no configured Wi-Fi network is available, AudioBooth creates:

```text
SSID:     audiobooth_xx
Password: 0123456789
```

`xx` is the two-digit booth ID from `00` through `15`. An open ID jumper is
logical 0 and a jumper connected to ground is logical 1. `ID0` is bit 0 and
`ID3` is bit 3. The same `audiobooth_xx` name is advertised as the device
hostname when AudioBooth joins a configured Wi-Fi network.

```text
PIN_BOOTH_ID0 = GPIO_NUM_39;
PIN_BOOTH_ID1 = GPIO_NUM_40;
PIN_BOOTH_ID2 = GPIO_NUM_41;
PIN_BOOTH_ID3 = GPIO_NUM_42;
```

Connect a phone or computer, then open the IP address printed in Serial. The
usual AP address is `http://192.168.4.1/`.

Existing-network credentials can be supplied as defaults in
`include/wifi_config.h`, or through **Wi-Fi for next reset** on the website.
Web-entered values are stored in ESP32 NVS and override compiled defaults.
They take effect after reset without interrupting the current connection.
Passwords may be empty for open networks; otherwise use 8–63 characters.

AudioBooth makes one initial connection attempt. If it times out, fallback AP
mode remains selected for that boot.

## Managing files through the website

Keep the handset down while using the website. Playback and web storage access
are never permitted simultaneously. 

### Browse and create directories

Select a directory name to open it. It becomes the destination for uploads.
Use **Parent directory** to move upward.

To create a directory:

1. Open its intended parent.
2. Enter a name under **Create directory**.
3. Select **Create**.
4. Select its name to open it.

### Upload files

1. Open the destination directory.
2. Choose a file under **Upload to this directory**.
3. Select **Upload**.
4. Wait for 100% and for the page to reload.

An existing file with exactly the same name is always overwritten. Do not
lift the handset, reset, remove SD or disconnect power during upload. If the
handset is lifted, the upload is rejected and a partial new file is removed.

### Delete and download

- **Delete** permanently removes a file; this cannot be undone.
- The website does not currently delete directories.
- On a configured existing Wi-Fi network, select a filename to download it.
- Download is disabled in fallback AP mode.

### Set the playback volume

The **Playback volume** section contains a slider from 0% to 100%. Move the
slider to the required level and select **Save volume**. The selected value is
used for every MP3 file and remains stored after reset or power-off. If no
volume has ever been saved, the default is 40%. The slider shows the current
stored value whenever the page is opened. At 100%, both the decoder output and
the configured ES8311 playback level can reach their full output.

Volume can only be changed while the handset is down, like all other website
functions. A value of 0% mutes playback; it does not stop or pause the MP3.

## Configuring `keyflow.txt`

Every menu directory may contain MP3 files, subdirectories and a local
`keyflow.txt`. When that directory becomes active, its configuration is read.
If `keyflow.txt` or `ini=` is absent, `main.mp3` is the initial file.

Example root configuration:

```ini
ini=welcome.mp3
1=information.mp3
2=stories
3=/opening_hours
no_key=silence
keypad=1
```

This plays `welcome.mp3` initially, makes key `1` play a file, key `2` enter a
relative directory, and key `3` enter an absolute directory. Spaces around
names and values are ignored. Lines starting with `;` are comments.

Supported keys:

```text
0 1 2 3 4 5 6 7 8 9 * # A B C D
```

An unmapped key does nothing and leaves current playback unchanged.

### File targets

File in the active directory:

```ini
1=information.mp3
```

Absolute and relative paths:

```ini
2=/shared/message.mp3
3=local_audio/message.mp3
```

Documented brace form:

```ini
4={/shared/}message.mp3
```

### Directory targets

A value that is not an MP3 target is interpreted as a directory:

```ini
5=stories
6=/opening_hours
```

`stories` is relative to the active directory. `/opening_hours` is relative to
root. A non-empty selected directory becomes active and its configuration is
processed. An empty or missing target triggers error handling while the old
directory remains active.

## End-of-file behavior

`no_key` is applied after a file ends and no key is pressed during its delay.

```ini
no_key=silence
no_key=repeat, 5
no_key=next, 3
```

- `silence`: wait indefinitely; also the default when omitted.
- `repeat, 5`: replay the last file after five seconds.
- `next, 3`: play the next MP3 after three seconds.

`next` uses case-insensitive alphabetical order and loops after the last MP3.
Directories, non-MP3 files and `error.mp3` are excluded. Omitting the number
means zero seconds.

## Error handling

For an invalid file or directory, AudioBooth searches for `error.mp3` in the
active directory and then each parent up to root. The active directory does
not change. After the error message, it tries the active directory's
`ini`/`main.mp3` again. Always provide a valid root `/error.mp3`.

## Keypad layouts

Only root `keyflow.txt` should define the layout.

### Layout 1 — standard layout (default)

```ini
keypad=1
```

| | Column 0 | Column 1 | Column 2 | Column 3 |
|---|:---:|:---:|:---:|:---:|
| **Row 0** | 1 | 2 | 3 | A |
| **Row 1** | 4 | 5 | 6 | B |
| **Row 2** | 7 | 8 | 9 | C |
| **Row 3** | `*` | 0 | `#` | D |

Use layout 1 for the currently fitted keypad.

### Layout 2 — rotated layout

```ini
keypad=2
```

| | Column 0 | Column 1 | Column 2 | Column 3 |
|---|:---:|:---:|:---:|:---:|
| **Row 0** | 1 | 4 | 7 | `*` |
| **Row 1** | 2 | 5 | 8 | 0 |
| **Row 2** | 3 | 6 | 9 | `#` |
| **Row 3** | A | B | C | D |

If `keypad` is absent or has a value other than `2`, layout 1 is used. Layout
2 must therefore be selected explicitly with `keypad=2`.

### Pinout

Keypad GPIO assignments:

```text
PIN_ROW0 = GPIO_NUM_4
PIN_ROW1 = GPIO_NUM_5
PIN_ROW2 = GPIO_NUM_6
PIN_ROW3 = GPIO_NUM_7

PIN_COL0 = GPIO_NUM_15
PIN_COL1 = GPIO_NUM_16
PIN_COL2 = GPIO_NUM_17
PIN_COL3 = GPIO_NUM_18
```

## Complete example

```text
/
|-- keyflow.txt
|-- main.mp3
|-- welcome.mp3
|-- information.mp3
|-- error.mp3
|-- stories/
|   |-- keyflow.txt
|   |-- main.mp3
|   |-- story_01.mp3
|   |-- story_02.mp3
|   `-- error.mp3
`-- opening_hours/
    |-- keyflow.txt
    |-- hours.mp3
    `-- error.mp3
```

Root `keyflow.txt`:

```ini
ini=welcome.mp3
1=information.mp3
2=stories
3=/opening_hours
no_key=silence
keypad=1
```

`/stories/keyflow.txt`:

```ini
ini=main.mp3
1=story_01.mp3
2=story_02.mp3
no_key=next, 5
```

`/opening_hours/keyflow.txt`:

```ini
ini=hours.mp3
no_key=repeat, 10
```

## Operational checklist

1. Format and populate SD, or upload to internal storage.
2. Provide root `main.mp3`, or a valid `ini=` target.
3. Provide root `error.mp3`.
4. Insert or replace SD while powered off, then reset/power up.
5. Check Serial to confirm whether SD or FFat was selected.
6. Keep the handset down while managing files.
7. Test every configured key, directory and error path before public use.

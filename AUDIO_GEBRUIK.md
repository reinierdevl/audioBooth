# Audio Usage and Key Flow

## 1. Storage

The first version uses FFat in the internal flash memory of the ESP32-S3
N16R8. An SD card is not required.

The root of the file system contains at least:

```text
/
|-- main.mp3
|-- error.mp3
`-- keyflow.txt
```

- `/keyflow.txt` contains `ini=main.mp3`. This determines which file is played
  when the handset is lifted.
- `/main.mp3` is the default initial file, but its filename may be changed.
- `/error.mp3` contains the error message for the root directory.
- `/keyflow.txt` also defines the valid key choices at the root level.
- The root directory follows the same rules as all subdirectories.

## 2. Choice directory structure

A key entry in `keyflow.txt` supports two types of actions:

1. Play an MP3 file from the current directory.
2. Enter another directory, load its `keyflow.txt`, and play its local
   `ini` file.

Every referenced directory contains at least:

1. its own `keyflow.txt`;
2. the MP3 file selected by `ini=`;
3. an `error.mp3` file for local error handling.

MP3 filenames are unrestricted except for the reserved name `error.mp3`. An
MP3 filename does not need to match the directory name.

Example:

```text
/
|-- main.mp3
|-- error.mp3
|-- keyflow.txt
|-- information/
|   |-- keyflow.txt
|   |-- error.mp3
|   |-- welcome_information.mp3
|   `-- opening_hours/
|       |-- keyflow.txt
|       |-- error.mp3
|       `-- hours_today.mp3
`-- stories/
    |-- keyflow.txt
    |-- error.mp3
    |-- stories_introduction.mp3
    `-- short/
        |-- keyflow.txt
        |-- error.mp3
        `-- short_story.mp3
```

A directory name identifies a key-flow node. Its local `keyflow.txt`
determines which freely named MP3 file is played first.

## 3. References

Every `keyflow.txt` starts with `ini=`. This entry selects the MP3 file that
is played first when that directory becomes active.

Example `/keyflow.txt`:

```ini
ini=main.mp3
1=extra_explanation.mp3
2=/information
no_key=next
```

This means:

- `/main.mp3` is played when the root session starts;
- key `1` plays `/extra_explanation.mp3` without changing directory;
- key `2` activates `/information`;
- when the audio finishes without a valid key choice, the next normal MP3
  file in the root directory is played in alphabetical order.

Example `/information/keyflow.txt`:

```ini
ini=welcome_information.mp3
1=more_information.mp3
2=/information/opening_hours
no_key=repeat
```

When `/information` becomes active, the following file is played:

```text
/information/welcome_information.mp3
```

Key `1` plays the following file without changing directory:

```text
/information/more_information.mp3
```

Key `2` activates:

```text
/information/opening_hours
```

Example leaf node `/information/opening_hours/keyflow.txt`:

```ini
ini=hours_today.mp3
no_key=silence
```

A `keyflow.txt` without key entries is a valid leaf node.

## 4. `keyflow.txt` syntax

### Initial audio file

```ini
ini=welcome.mp3
```

- `ini` is mandatory.
- Its value is the name of an MP3 file in the same directory as
  `keyflow.txt`.
- The file is played when the directory becomes active.

### A key plays a file

```ini
1=file_1.mp3
```

- A value ending in `.mp3` references a file in the current directory.
- The active directory and active `keyflow.txt` do not change.
- The currently playing file is stopped and the selected file is started.

### A key activates a directory

```ini
2=/next_directory
```

- A value beginning with `/` references a directory from the FFat root.
- That directory must contain a `keyflow.txt`.
- The new `keyflow.txt` becomes active.
- The MP3 file selected by its local `ini=` entry is started.

A nested directory is written as a complete path:

```ini
2=/information/opening_hours
```

### No key was pressed

`no_key` determines what happens when the current MP3 file ends without a
valid key choice.

Exactly three values are supported.

Play the next normal MP3 file from the current directory in alphabetical
order:

```ini
no_key=next
```

The player builds a case-insensitive, alphabetically sorted list of all MP3
files in the active directory. After the last file, it starts again with the
first file. This continues until a valid key is released or the handset is
put down.

`error.mp3` is reserved for error handling and is excluded from this list.
The file selected by `ini=` and all other MP3 files in the directory are
included.

Repeat the most recently played MP3 file:

```ini
no_key=repeat
```

Remain silent and wait for a valid key or for the handset to be put down:

```ini
no_key=silence
```

Valid key choices remain active during all three `no_key` modes.

## 5. Key-flow rules

- The root directory is the initial level.
- When the handset is lifted, `/keyflow.txt` is loaded and its `ini` MP3 file
  is started.
- `/keyflow.txt` defines the valid choices at the root level.
- Every `keyflow.txt` must select its initial MP3 file with `ini=`.
- A key value ending in `.mp3` plays a file from the current directory.
- A key value beginning with `/` activates that directory.
- Every activated directory must contain a `keyflow.txt`.
- MP3 filenames are freely selectable, except for the reserved
  `error.mp3`.
- Directory references are absolute paths starting at the FFat root.
- References must not contain `..`.
- The key flow supports at most four consecutive directory choices from the
  root.
- A key without an entry is ignored and the current MP3 continues playing.
- A valid key stops the current MP3 and performs the associated file or
  directory action.
- A choice is processed only after the key has been released and its release
  state has passed debounce validation.
- `no_key` accepts only `next`, `repeat`, or `silence`.

## 6. Session behavior

### Lifting the handset

1. Set the active directory to `/`.
2. Load `/keyflow.txt`.
3. Play the initial MP3 selected by `ini=`, normally `/main.mp3`.
4. Continue scanning the keypad while audio is playing.

### Making a choice

1. The user presses and releases a valid key.
2. Stop the current MP3.
3. For an MP3 reference, start the file from the current directory.
4. For a directory reference, activate the referenced directory.
5. Load the local `keyflow.txt`.
6. Start the MP3 selected by its `ini=` entry.

### Putting down the handset

1. Stop the active MP3 immediately.
2. Clear the active directory and key-flow state.
3. Ignore key events.
4. Wait until the handset is lifted again.

The next session starts again with `/keyflow.txt` and its root `ini` MP3
file.

## 7. Error handling

Every directory must contain a reserved file named exactly:

```text
error.mp3
```

This file is never played by `no_key=next`. It is used exclusively for error
handling.

### Error in a normal file, key target, or `keyflow.txt`

1. Stop the current playback action.
2. Play `error.mp3` from the active directory.
3. Reload the active `keyflow.txt`.
4. Play the file selected by the local `ini=` entry.

If reloading `keyflow.txt` fails again or its `ini=` entry cannot be read,
continue with the parent-directory recovery procedure.

### Error in the local `ini` file

If the file selected by `ini=` is missing, invalid, or cannot be decoded, the
player must not repeatedly retry the same recovery path:

1. Move one directory level up.
2. Play `error.mp3` there if it exists and is playable.
3. Load the `keyflow.txt` of that parent directory.
4. Play the file selected by its `ini=` entry.

### Missing or defective `error.mp3`

If `error.mp3` in the active directory is missing or cannot be played:

1. Move one directory level up.
2. Try the local `error.mp3` there.
3. Continue with that directory's `ini` file.

Repeat this procedure as necessary until the root directory is reached.

### Error in the root directory

The root also contains:

```text
/keyflow.txt
/error.mp3
```

The `ini=` entry in `/keyflow.txt` determines the initial audio file when the
handset is lifted. If recovery through both `/error.mp3` and the root `ini`
file is impossible, stop all audio and remain silent until the handset is put
down. This prevents an infinite error loop.

## 8. Recommended MP3 format

For speech played through a telephone handset:

```text
Container/codec: MP3
Channels:        mono
Sample rate:     22,050 Hz
Bitrate:         32 kbit/s CBR
```

This is the recommended balance between intelligibility and storage
duration.

Alternatives:

| Format | Estimated total duration with 11.5 MiB |
|---|---:|
| Mono, 44.1 kHz, 64 kbit/s | approximately 25 minutes |
| Mono, 22.05 kHz, 48 kbit/s | approximately 33 minutes |
| Mono, 22.05 kHz, 40 kbit/s | approximately 40 minutes |
| Mono, 22.05 kHz, 32 kbit/s | approximately 50 minutes |
| Mono, 16 kHz, 24 kbit/s | approximately 67 minutes |
| Mono, 16 kHz, 16 kbit/s | approximately 100 minutes |

The actual total duration depends on the final partition table, file-system
overhead, MP3 metadata, and firmware size.

## 9. File validation

When loading a node, the firmware verifies at least that:

- the directory exists;
- the local `keyflow.txt` exists;
- `ini=` contains a safe MP3 filename;
- the selected MP3 file exists;
- the filename ends in `.mp3`;
- the maximum of four directory-choice levels is not exceeded;
- every key references either an existing MP3 file or an existing directory;
- `no_key` contains `next`, `repeat`, or `silence`;
- references do not contain `..`;
- `error.mp3` is excluded from the normal `next` playlist.

An error in one node must never crash or block the firmware. Recovery follows
the parent-directory procedure described above.

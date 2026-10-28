# DOS settings profiles

On the device screen, press **F5** to export the current settings or **F6** to
import a profile. Both help labels can be clicked with an installed DOS mouse
driver. The default filename is **OPENTAPE.CFG**, an ordinary 8.3 filename.
Enter a different short filename/path if required; the field limit is 40
characters. F2 proceeds to a separate review; Esc discards filename editing.
Only a fresh Y in English or J in German performs the operation. Any other key
cancels the review. The result consumes its dismissal key/click and returns to
the device screen; it cannot start a second operation.

Profiles contain all settings from **F** and **P**: backup source, archive path,
volume, file/tree/set mode and configured/disabled state, plus the five independent
preview/restore/comparison/import-list/export-list paths. Import replaces both
groups together, including empty fields that disable their actions. It performs
no source existence/destination checks beyond those needed to load the profile;
the later backup/restore services still validate their own source and target.

Profiles contain **no tape/backend selection, device state, catalogue marks,
replacement grants, search query or write authorization**. Import does not probe,
rewind, read or write tape, and never starts backup or restore. B/C/W/K/I/E still
require their normal separate confirmation. F5/F6 are device-screen actions;
at an outstanding device confirmation they cancel that question and are consumed.
They do not edit a frozen catalogue operation. No profile is loaded automatically
at startup and there is no profile command-line option. Defaults/startup arguments
remain the initial values until an explicit import.

The edited filename is remembered for this UI session only after a confirmed
attempt. Cancelling before confirmation leaves that remembered name unchanged.
Filename validation follows the project's DOS short-name policy, rejecting
wildcards, control characters and tape-device stems, including extensions and
optional `$`. Thus `MT0`, `RMT0.CFG`, `NMT0.CFG`, `NRMT0$.CFG` and matching custom
letter variants are refused before opening. The same shared stem rule now
protects all DOS regular-source opens, including canonicalized dot/parent
forms; a DOS device close must not cause
a rewind during a profile operation. Directory/regular-file checks still apply
in the guarded DOS backend. Parent directories must already exist.

## Export and import guarantees

Export creates a **new file only**. An existing target is preserved and reported
as an error; choose another filename to export an updated profile. The existing
protected selection-output backend validates the destination and parent, refuses
an existing `OT$LIST.TMP`, and creates that staging file exclusively. The profile
must be written exactly and closed successfully, then independently reopened,
checked for exact length/CRC/EOF, snapshot-rechecked and closed before rename to
the requested filename. Rename does not replace an existing target. Failure does
not publish a partial profile; a failed staging file may remain for inspection.
No automatic cleanup removes an unfamiliar existing temporary file.

Import opens a guarded regular file read-only, requires exactly 337 bytes,
checks exact EOF, rechecks its source snapshot and checks close. Only then does
it validate the complete versioned encoding, both checksums and every field.
A missing/truncated/oversized/changed file, read/close error, malformed version,
checksum failure or invalid setting leaves **all current settings unchanged**.
Neither partial data nor a valid-looking prefix can update one settings group.

## Version 1 encoding

This is a binary settings format, separate from the tape archive and from the
text selection-list format. It is independent of compiler struct layout,
16/32-bit integer size and UI language. Edit settings with F/P, then export
another profile rather than modifying bytes in a text editor.

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 4 | ASCII `OTCF` |
| 4 | 2 | Little-endian version: 1 for File/Directory, 2 for Set |
| 6 | 2 | Little-endian total length, 337 |
| 8 | 4 | CRC32 of bytes 16 through 336 |
| 12 | 4 | CRC32 of bytes 0 through 11 |
| 16 | 1 | Backup configured: 0/1 |
| 17 | 1 | Mode: v1 File/Directory = 0/1; v2 Set = 2 |
| 18 | 41 | Source |
| 59 | 41 | Archive path |
| 100 | 32 | Volume |
| 132 | 41 | Preview destination |
| 173 | 41 | Restore destination |
| 214 | 41 | Comparison directory |
| 255 | 41 | Selection input filename |
| 296 | 41 | Selection output filename |

Strings are terminated and zero-padded; nonzero bytes after a terminator are
rejected. A disabled backup requires an empty source. Source/path/volume and
path settings use their existing syntax checks; embedded control characters
are forbidden. CRC detects accidental damage; it does not authenticate a profile.
Unknown versions, sizes and flag values are rejected without guessing defaults.

`core/settings.c` owns the pure codec and validation. `src/settings_dos.c` owns
explicit file import/export and commit timing. `src/settings_dos_ui.c` provides
filename editing, one-shot confirmation and results through the shared QUILIB
field/mouse adapter. `ot_dos_file_save_blob` reuses the existing protected DOS
file publication paths, adding independent staged readback for small blobs.
There is no copied widget library or second tape-write implementation.

## Tests

```text
make OPENASPI=../OpenAspi dos-settings-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-settings-smoke
```

The host/`SETTEST.EXE` codec tests flip every encoded byte, test CRC-valid invalid
fields/versions/padding, exact field limits, disabled settings and unchanged
output on failure. Host runs also use ASan/UBSan. The QEMU tests configure F/P
interactively to respect DOS command-tail limits, then export, alter both groups,
import and inspect restored values. Python independently constructs the expected
little-endian/CRC format; exported bytes must match exactly in either language.

Cases cover filename/review cancellation, consumed result keys, existing target
refusal, missing/corrupt profiles, a pre-existing staging file, empty-profile
disabling and a tape alias with a shadow FAT filename. Failure/read-only cases
require byte-identical whole DOS disk images. Every profile fixture requires
zero tape ASPI commands; successful exports leave no staging file. The mouse
case uses the [local CuteMouse fixture](dos-mouse.md). No physical tape is used.

## Backup-set profiles

[Set mode](backup-sets.md) uses version 2 with the same 337-byte layout and
checksums. It stores the manifest filename and common base, not the list itself.
File/Directory exports remain version 1. New readers accept both; old readers
reject v2. Version/mode mismatches are rejected atomically, and importing Set
configuration does not open its manifest or grant a backup operation.

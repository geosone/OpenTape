# OpenTape native volume format version 1

The native format is a documented single-partition sequential stream inspired
by the recovery model of Bacula/Bareos. All integers are unsigned little-endian
values. On-tape data never contains compiler-packed C structures.

The first logical block contains a volume-label record. Backup sessions contain
start, file metadata/data/end and session-end records. A tape filemark closes a
completed session. The last writable session ends with a volume-end record.
Each logical block is normally one variable-length physical tape record; fixed-
block transports use the same serialized bytes in 32 KiB units.

## Block header

Every logical block begins with a 36-byte header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII `OTB1` |
| 4 | 2 | format version (`1`) |
| 6 | 2 | header size (`36`) |
| 8 | 4 | total block size including header |
| 12 | 4 | monotonically increasing block number |
| 16 | 4 | session identifier; zero for volume-level blocks |
| 20 | 4 | session creation time |
| 24 | 2 | record count |
| 26 | 2 | block flags |
| 28 | 4 | IEEE CRC-32 of the payload |
| 32 | 4 | IEEE CRC-32 of bytes 0 through 31 |

The recommended total block size is 32,768 bytes. Version 1 is limited to
65,535 bytes so DOS and the ASPITAPE character-device interface can transfer a
whole block in one request.
New volumes number their blocks consecutively from zero. The label block has
session identifier zero. Version 1 currently defines only the listed block
and record flag bits; strict readers reject other flag bits.

## Record header

The block payload is a sequence of 20-byte record headers followed immediately
by their data fragments:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | record type |
| 2 | 2 | fragment flags (`FIRST`, `LAST`) |
| 4 | 4 | following fragment size |
| 8 | 4 | object identifier |
| 12 | 4 | byte offset within the logical object |
| 16 | 4 | IEEE CRC-32 of the fragment data |

Version 1 reserves these record types: volume label, session start, file
metadata, file data, file end, session end, volume end and catalogue checkpoint.
Unknown record types can be skipped using the stored fragment size.

This document freezes the common framing, volume label, and the version-1
DOS file/session payloads below. Volume-end and catalogue-checkpoint payloads
remain unfrozen. No native backup writer or restore implementation exists yet;
framing/codec test data must not be mistaken for completed backups.

## Volume label payload

The fixed 88-byte label payload is stored in the first record:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | ASCII `OPENTAPE` |
| 8 | 2 | format version (`1`) |
| 10 | 2 | label size (`88`) |
| 12 | 2 | flags |
| 14 | 2 | reserved, zero |
| 16 | 4 | volume sequence within a set |
| 20 | 4 | UTC creation time |
| 24 | 16 | binary volume UUID |
| 40 | 16 | binary volume-set UUID shared by manually exchanged tapes |
| 56 | 32 | zero-padded volume name |

## Recovery rules

A catalogue scanner reads blocks in sequence, validates both CRCs, skips a
damaged physical record when the tape transport permits it, and resumes at the
next `OTB1` header. The planned stateful recovery layer will interpret typed
records to reconstruct volumes, sessions and files. Catalogue checkpoints are
advisory; recovery never depends on them. A missing session-end or file-end
record marks that object incomplete rather than hiding previous valid data.

Formatting, overwriting and recycling policy is deliberately outside the byte
format and always requires an explicit user action.

## Strict framing scanner

`core/native_scan.c` validates complete logical blocks. It checks both block
CRCs, consecutive block numbers starting at zero, exact record counts and
payload consumption, per-fragment CRCs, known flag bits, and fragment-offset
overflow. A fragment marked FIRST must start at offset zero. Unknown record
types are skipped only after their lengths and CRCs are validated.

The first record must be the complete 88-byte volume label, marked FIRST|LAST
at offset zero. Label magic/version/size and reserved bytes are checked;
duplicates and misplaced labels are rejected. Scanner counters and label
are committed only after the entire block passes, so a corrupt later record
cannot publish an earlier label from the same block.

This strict pass also decodes known file/session payloads and checks their
version, exact size, reserved fields, field ranges and record-ID/fragment shape.
File/session records need a nonzero block session ID; session start/end use
object ID zero, while file metadata/data/end use nonzero object IDs. Control
payloads must be whole FIRST|LAST records at offset zero; FILE_DATA may fragment.
Volume labels use object ID zero. Unknown record types retain skip-by-length
behavior after framing/CRC checks.

It stops at the first error. It does not yet resynchronize after corruption,
reconstruct objects/fragments across blocks, enforce session/file ordering or
cross-check completion totals/whole-file CRCs. It cannot establish that a
backup is complete. A separate [stateful verifier](archive-verification.md)
now checks the sequential file/session profile across blocks. Block FIRST/LAST
flags alone are not used as evidence of complete backup or volume termination.
The parser accepts individual blocks up to the format's 65,535-byte limit;
the initial DOS CLI uses a 32 KiB transport buffer.

`OPENTAPE --scan [--device NRMT0]` connects this scanner to the raw reader.
It reads at most 16 physical records from the current position, without
rewinding or writing. Position the tape at the volume beginning first. It
displays a sanitized label and validated block/record counts. A filemark ends
this bounded scan, not proof of whole-volume completeness. An empty tape file
without a label is an error. A label containing terminal control bytes cannot
inject screen commands through the displayed name.

Exit codes are `0` valid framing through the first filemark, `1` bad arguments,
`2` transport/format error, `3` record limit reached without confirming EOF.
Mandatory fixed-block transports are rejected before data reads: reassembly
of serialized logical blocks from physical fixed units is not implemented yet.
The raw `--read-test` diagnostic remains available independently of this format
restriction. The native writer and damaged-record recovery are still pending.

## File and session payloads, version 1

These payloads start with a little-endian 16-bit version (`1`), independent
of the block-format version. They are serialized explicitly, never by copying
C structs. Fixed-size decoders require exact lengths. Reserved bits/fields must
be zero. Failed decoding does not modify the destination object. The shared
little-endian primitives also serve the existing block and label codecs.

### Session start (56 bytes)

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | payload version |
| 2 | 2 | reserved flags, zero |
| 4 | 4 | creation UTC seconds |
| 8 | 16 | session UUID |
| 24 | 32 | zero-padded session display name |

A full 32-byte name need not contain NUL; after the first NUL all remaining
bytes must be zero. Names are display data, never filesystem paths. The writer
must assign a nonzero session ID in containing block headers, use the session
creation time as their session time, and emit SESSION_START before its files.
Those cross-record invariants are enforced by the separate sequential verifier,
not by the framing scanner alone.

### File metadata (20-byte prefix plus path)

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | payload version |
| 2 | 2 | kind: `1` regular file, `2` directory |
| 4 | 2 | DOS attributes, only mask `0037h` allowed |
| 6 | 2 | path byte length, `1..255` |
| 8 | 4 | file byte size; zero for directories |
| 12 | 2 | packed DOS modification time |
| 14 | 2 | packed DOS modification date |
| 16 | 4 | reserved, zero |
| 20 | variable | relative ASCII DOS-8.3 path, no NUL terminator |

The directory attribute (`10h`) must agree with kind; volume-label/reserved
attributes are rejected. Date/time are DOS local modification values, with
two-second time resolution, not UTC. Both zero means unknown. Otherwise the
date must exist (including Gregorian leap-year rules) and the time must be
valid. The 32-bit size agrees with the framing's 32-bit fragment offsets;
this payload version does not represent files beyond `FFFFFFFFh` bytes.

Paths preserve case and use `/` separators. Every component has a 1..8-byte
base and optional 1..3-byte extension; allowed characters are ASCII letters,
digits, `_`, `-`, `$`, `~`, plus the separating dot. Empty components, `.`/`..`,
absolute paths, drive prefixes, backslashes, spaces and control bytes are
rejected. Device basenames CON, PRN, AUX, NUL, CLOCK$, CONIN$, CONOUT$, COM0..9
and LPT0..9 are rejected case-insensitively, even with an extension.
Long names/non-ASCII paths need a future payload version; the initial DOS
writer will not silently rename or truncate them. Lexical validation is not
a restore sandbox: destination collisions, case folding and filesystem links
still need checks in the future restore implementation.

Metadata assigns a nonzero object ID unique within the session. Directory
metadata has no FILE_DATA or FILE_END. Empty regular files may have no data
fragments but still require FILE_END. Regular FILE_DATA records contain raw
file bytes; fragments use the same object ID and contiguous byte offsets,
starting with FIRST at zero and ending with LAST. No physical/block padding
is included in logical file bytes.

### File end (16 bytes)

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | payload version |
| 2 | 2 | reserved flags, zero |
| 4 | 4 | total file byte size |
| 8 | 4 | IEEE CRC-32 of complete file bytes in order |
| 12 | 4 | number of FILE_DATA fragments |

The object ID identifies its regular file. Size must match metadata and the
sum of data fragments. Empty files have size/CRC/fragment count zero. A missing
FILE_END leaves the file incomplete; successful payload decoding alone does
not verify file completion or the whole-file checksum.

### Session end (20 bytes)

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | payload version |
| 2 | 2 | result: `0` complete, `1` incomplete |
| 4 | 4 | number of completed regular files |
| 8 | 4 | number of directory metadata objects |
| 12 | 4 | total completed-file bytes, low 32 bits |
| 16 | 4 | total completed-file bytes, high 32 bits |

The 64-bit aggregate is represented as two words so DOS does not need a native
64-bit C type. Source read errors or changed/incomplete files must never be
reported as a complete session. A missing end leaves a session incomplete.
The framing scanner accepts the shape of either result. The sequential verifier
checks ordering, totals and file CRCs and rejects incomplete sessions; this still
does not establish whole-volume completeness or successful restore.

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

This document freezes the common framing and the volume-label payload. The
payload schemas of the remaining typed records will be frozen together with
the first backup writer, before those records are written to real media. This
keeps early test tapes from being mistaken for complete version 1 backups.

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
next `OTB1` header. Once the typed payload schemas are frozen, those records
will reconstruct volumes, sessions and files. Catalogue checkpoints are
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

This strict pass stops at the first error. It does not yet resynchronize after
corruption, reconstruct objects/fragments across blocks, interpret file/session
payload schemas, or establish that a backup is complete. Block FIRST/LAST
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
restriction. The payload schemas of file/session records are still unfrozen.

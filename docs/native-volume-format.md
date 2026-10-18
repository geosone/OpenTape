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

# Offline catalogue files

F8 in the verified catalogue saves all entries to a regular DOS file. F9 in
both the device and catalogue views loads such a file for browsing without a
cartridge. The default is `OPENTAPE.CAT`; the filename editor accepts up to
40 characters and an 8.3 leaf. Both builds (US/GR) use the shared QUILIB fields,
keyboard and mouse path. The device help shows `[F8 >] [F9 <]`; F8 requires an
open catalogue and otherwise has no effect. In the catalogue the Back/F8 pair
provides export, and F9 remains available from the keyboard.

F2 leaves the editor for a separate review. Only a fresh Y (US) or J (GR)
performs the operation. Any other confirmation key cancels and is consumed;
the result dismissal also cannot start another action. Cancelled editing does
not change the remembered filename. A confirmed attempt remembers its name,
even on failure. There is no automatic loading or saving at startup/exit.

The [batch command interface](catalog-cli.md) also exports checked tape
catalogues, lists saved files and explicitly opens them in the DOS interface.

## Snapshot and authority

The snapshot contains the volume label and each entry's session/object IDs,
path, kind, size, DOS attributes and timestamps. It contains no file contents,
checksums of individual files, extraction marks, replacement grants, device
state or proof of the currently mounted cartridge. A valid metadata CRC is
neither authentication nor a completed backup verification.

Import replaces the in-memory catalogue only after the entire file, exact EOF,
source snapshot recheck and close succeed. Failure/cancellation leaves the old
catalogue, label, marks, grants, cursor and live/offline provenance unchanged.
A successful import clears every mark and grant and starts at the first entry.
Export leaves the source catalogue and annotations unchanged.

Loaded files display **OFFLINE metadata**, their saved volume name and an
explicit missing-tape-identity notice. Paging, horizontal scrolling, search,
file/directory marks and path-list import/export work in memory or on ordinary
files. C preview, W restore, K comparison and O replacement intent are blocked
before any tape operation. To operate on tape again, return with B/Esc, rewind
as appropriate and read a fresh verified catalogue with L. The saved metadata
is never silently associated with the selected alias.

## Disk publication

Export first validates/measures the entire model. It then exclusively creates
`OT$LIST.TMP` in the destination directory, streams small chunks, checks exact
writes and close, independently reopens/readbacks the temporary file with
length/CRC/EOF/snapshot checks, then renames it to the destination. Existing
targets or staging files are refused. Parent directories must exist. Failed
staging is retained for inspection; no automatic cleanup/retry occurs. DOS
regular-file guards reject character devices and tape stems before open,
including extension-bearing rewind aliases such as `RMT0.CAT`.

## Wire format, version 1

All integers are unsigned little endian. No pointers, structure dumps or
annotations are serialized. The maximum is 256 entries and 73,592 bytes.
Individual reads/writes remain small; neither encoder nor decoder allocates a
whole image, so a catalogue exceeding 64 KiB is supported by DOS large model.

| Header offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | `OTCAT1\r\n` |
| 8 | 2 | Version = 1 |
| 10 | 2 | Header size = 32 |
| 12 | 2 | Entry count, 0–256 |
| 14 | 2 | Reserved, zero |
| 16 | 4 | Payload length |
| 20 | 4 | CRC32 of the complete payload |
| 24 | 4 | Reserved, zero |
| 28 | 4 | CRC32 of header bytes 0–27 |

The payload starts with the existing 88-byte volume-label codec. Each entry
has a 12-byte prefix: metadata length (u16), zero reserved (u16), session (u32),
object (u32), followed by the existing variable-length file-metadata codec.
Both IDs must be nonzero; sessions cannot decrease; objects strictly increase
within a session. Duplicate paths across objects/sessions remain independent
versions; path-only extraction/list operations retain their ambiguity checks.
Reject wrong lengths/counts/reserved fields/checksums, bad metadata or IDs,
truncation and trailing bytes before replacing the previous model. Provisional
nodes are freed on every rejected import. The reader does not recover corrupt
files or infer missing metadata.

## Verification

`test_catalog_file` and DOS `CATTEST.EXE` exercise a 256-entry image exceeding
64 KiB using separately allocated 512-byte fixture chunks, 13-byte short reads,
roundtrips without annotations, empty catalogues, malformed header/body and
recomputed-checksum invalid entries. Read, source-change, close, short-write
and publication failures check atomic import and exact ownership callbacks.

The `ui-catfile*` QEMU cases drive the real editor/confirmation/catalogue UI,
with keyboard and PS/2 mouse, check refusal of existing targets/staging and
invalid/missing/device filenames, and verify that offline C/W/K/O issue no
additional CDBs. An independent Python parser checks the exported DOS file.
All integration media are fixture memory and private floppy images.

`ui-catfile-offline` starts from an independently encoded catalogue without any
seed/probe/status call, browses/searches it, and checks cancellation and blocked
operations with an exact total of zero CDBs. `ui-catfile-large` runs `CATIO.EXE`
through the production DOS filesystem backend: 256 long paths create a file
above 64 KiB, checked staging readback and a second import succeed, existing
output and tape aliases are refused, and the GUI loads/pages/scrolls the last
entry. Its complete execution also requires zero CDBs. US and GR are covered.

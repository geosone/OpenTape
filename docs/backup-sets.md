# Backup sets: multiple source roots

A backup set combines selected files and directory trees into one ordinary
OpenTape session. It uses the existing source planner, writer, per-file CRCs,
finalization and native/legacy/fixed transports. It introduces no new tape
format. Independent readback is still required before relying on a backup.

Create a DOS text file, for example `SOURCES.LST`:

```text
# Type  DOS source       Relative archive target
F       C:\WORK\MAIN.BIN  MAIN.BIN
T       C:\DOCS           DOCUMENT
F       D:\NOTES.TXT      TEXT/NOTES.TXT
```

F means one regular file; T means a directory including its descendants and
empty directories. Letters are case insensitive. Separate the three fields
with spaces or tabs. Blank lines and lines whose first non-space character is
`#` are ignored. CRLF and LF are accepted, as is a final line without newline.
There are no quotes, wildcards, inline comments, variables or nested manifests.
A line must contain exactly three fields. DOS control-Z is rejected rather
than silently treated as end of input; NUL, bare CR and other control bytes
are also rejected. Source tokens cannot contain spaces, quotes, `*`, `?`, `<`, `>` or `|`.

Archive targets are validated relative 8.3 paths, at most 255 characters. Source
paths are at most 127 characters and resolve against **DOS's current directory**,
including when the manifest itself is in another directory. Use explicit drive
paths for reproducible batch files. Every expanded source is canonicalized by
the existing DOS backend before later emission. Device/tape aliases, including
extension-bearing aliases and canonicalized dot/parent forms, are refused.

## Command line

```text
OPENTAPE --backup-set SOURCES.LST --as DATA --device NRMT0 --overwrite --volume TEST
OPENTAPE --native --backup-set SOURCES.LST --as DATA --device NRMT0 --overwrite --volume TEST
OPENTAPE --native --fixed-block 512 --backup-set SOURCES.LST --as DATA --device NRMT0 --overwrite --volume TEST
```

The `--as` path is a common archive base: the example produces `DATA/MAIN.BIN`,
`DATA/DOCUMENT/...` and `DATA/TEXT/NOTES.TXT`. Implicit parent directories do not
require separate manifest entries. Final joined paths must fit the existing
255-character archive limit. Duplicate archive paths are refused regardless
of ASCII case. A regular file cannot be an ancestor of another file/directory;
`TARGET` as a file conflicts with `target/SUB`. Complete path-component matching
keeps `TARGET` distinct from `TARGET2`. Directory roots that map to the same
archive path are also refused, rather than merged implicitly. One physical
source may appear at several distinct archive targets.

Like file/tree backups, the explicit command writes at the current tape
position, requires `--overwrite` and does not rewind or retry automatically.
The CLI returns 0 after completed write/finalization/close, 1 for invalid command
configuration, and 2 for source/manifest/planning/tape failures. A malformed list
is a source failure (2). Format errors report -230, manifest bounds -231,
combined plan bounds -110 and archive-target conflicts -111. A successful write still needs independent readback.

## DOS interface

F opens the shared QUILIB settings editor. Select **Set** with the mouse or
cycle File → Directory → Set → File with F4. Enter the manifest in Source,
common archive base in Path and the volume name in Volume. F2 commits only
configuration; Esc discards the draft. The existing 40-character UI source/base
bounds still apply. Startup configuration also supports:

```text
OPENTAPE --ui --backup-set SOURCES.LST --as DATA --volume TEST
```

F7 reads/checks the manifest and all member metadata, builds the complete plan,
and reports combined file/directory and wide payload totals. It never reads
file payloads or acquires tape. Missing, malformed, conflicting or oversized
sets publish no partial counts. B reviews Set mode, the manifest/base/volume
and current-position warning, then requires a fresh Y (US) or J (GR). B always
rebuilds from the current list/sources; a prior F7 result is not reused as a
plan or grant. The existing progress observer displays combined totals and
individual current archive paths. Final success follows tape cleanup.

## Profiles and limits

F5/F6 preserve Set mode and its manifest filename, base and volume. The list
contents are not embedded. SET profiles use **OTCF version 2**, retaining the
337-byte layout, zero padding and two CRCs. Byte 17 has mode 2. Version 1 stays
canonical for File/Directory mode (byte 17 = 0/1); new readers accept both.
Old readers reject v2 instead of interpreting Set as a directory. A v1 profile
claiming mode 2, or a v2 profile claiming another mode, is invalid. Profile
import still grants no source/tape operation.

A manifest is bounded to 32,768 bytes, 64 roots and 385 bytes per line including
whitespace (excluding CR/LF). Expanded output is bounded to **256 total entries
across all roots**, counting directories. Each tree retains the existing
16-level descent limit and DOS source/path bounds. Comments count toward file
and line limits. More roots, combined overflow, excessive depth or allocation
failure aborts planning before tape acquisition. Plans use independently
allocated nodes, never one contiguous allocation above 64 KiB. The DOS UI stack
is 16,000 bytes to accommodate its dialogs plus set composition and tree walk.

## Transaction boundaries

The portable parser streams small chunks and owns checked EOF/snapshot recheck/
close. It builds a provisional list and replaces an initialized owned list only
after full success; failure preserves the old list. The DOS composer then
builds each member with the existing file/tree planner and merges validated
entries into an owned provisional combined plan. Any member, close, collision
or limit failure frees every provisional node and leaves the output plan empty.
Only a completely successful plan can reach tape acquisition. Public backup
services still require authorization == 1 before opening even the manifest.

Each regular source is independently reopened and checked against its planned
metadata during emission. There is no whole-source-set filesystem snapshot,
filesystem freeze, whole-session rollback, incremental backup policy or automatic
media continuation. Changes/failures after tape acquisition can leave an
incomplete session, under the existing backup failure rules.

## Verification

`test_backup_set` and DOS `BSETTEST.EXE` check 64-root and byte/line boundaries,
short reads, EOF, malformed fields, device stems, CRLF/control bytes, recheck/
close/read failures and preservation of an existing parsed set. Plan tests
cover file/descendant conflicts in both insertion orders and component boundaries;
profile/form tests cover mode 2, v1/v2 interoperability and atomic rejection.

The `dos-backup-set-smoke` QEMU matrix covers native, legacy and 512-byte fixed
write/readback; malformed/duplicate/prefix/device/canonical-alias inputs; combined
entry overflow; keyboard/mouse form choice, F7 totals, v2 profile export/import,
one-shot write confirmation and consumed result dismissal. Additional cases
capture actual intermediate progress and qualify 16 accepted/17 rejected tree
descents through Set mode. All source and reference bytes are private fixtures;
no physical tape or changer is used. Invalid/limit cases require zero CDBs;
positive CLI cases compare the full disk image and independently compare tape
contents with prepared references after rewind.

# Read-only DOS restore preview

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --dry-run
```

`--dry-run` runs the normal restore verifier and selection logic against a
read-only destination inspection backend. It does not create the destination
root, directories or staging/recovery files, open destination files for writing,
copy data, rename/delete files or change attributes/timestamps. It starts at the
current tape position and reads the complete supported archive prefix through
its first filemark. No tape data or changer state is written; tape position
does change by reading. Use [`--rewind`](tape-positioning.md) explicitly before
another BOT operation.

## Marked files in the DOS interface

```text
OPENTAPE --native --ui --preview-to C:\RECOVER
```

Load the verified catalogue with L, mark regular files with Space (A/N selects
all/none), then press **C**. The destination is initialized at startup or edited with **P** through
[session path settings](catalog-paths-ui.md). The field accepts up to 40 characters for the
DOS panel, subject to the existing local short-path validation. Omitting
`--preview-to` leaves this action disabled until a preview path is saved with P. `--selection-out` may be used alongside it.

The confirmation explicitly requests **rewind followed by a fresh archive read**.
Y confirms in English, J in German; any other key cancels and is consumed.
The selected paths are copied and checked for version/path ambiguity before
confirmation. Destination preparation and restore-filter validation happen
before any tape movement. A failed rewind stops the batch without a read or
retry. This batch uses the selected native/legacy backend and configured native
fixed-block size. It does not operate a changer.

The result shows planned new files, existing files kept and excluded archive
files. Existing directories are permitted for this inspection. Plain `[x]`
marks are extraction filters; they never infer replacement permission. The
separate [O grant](catalog-restore.md#per-file-replacement-with-o), shown as `[!]`,
lets C preview exact replacements. Confirmation reports the grant count and
results show planned replacements separately. C retains the grants for later
review and never creates a root, stages output or changes attributes. Missing selected paths, path conflicts,
archive corruption and tape close errors fail the preview. Counts remain
provisional on failure. All excluded archive data is also verified.

Any key returns from the result to the retained catalogue and marks. The
catalogue is an offline snapshot; the fresh read checks the current medium and
requires all selected paths, but does not prove the medium/session identity
matches the original catalogue. No persistent plan or later restore permission
is created. A real restore must independently validate its inputs and per-file
replacement permissions. The [W action](catalog-restore.md) separately confirms
restoration with `--restore-to`, using a new root or explicit O grants. Existing file contents are not compared by preview.

The quiet services share the CLI engine and return `ot_dos_restore_result`;
backend rows and console output are suppressed to preserve the QUILIB display.
`ot_dos_preview_selected` and its `ot_dos_restore_inspect` backend keep existing
files. `ot_dos_preview_replacing` accepts the separate immutable replacement
subset and uses the regular read-only policy backend; its `replaced` field
reports planned replacements. Both require full verification and successful close.

## Actions and completion

Output begins with an explicit provisional warning. Rows contain:

```text
Action ArchivedBytes ArchivePath
NEW               123 ROOT/FILE.BIN
REPLACE             0 ROOT/EMPTY.BIN
KEEP               55 ROOT/OLD.BIN
DIR                 0 ROOT/SUB
```

`NEW` means normal restore would attempt to create a missing selected file.
`REPLACE` means an existing selected file has explicit replacement permission.
`KEEP` means an existing selected file has no replacement permission and would
remain unchanged. The byte count is the **archived** length, not the length of
an existing destination or a disk-space estimate. `DIR` denotes a checked
explicit archive-directory entry; it does not mean that directory is absent
or that metadata would be replayed. Directory metadata replay is still pending.

NEW/REPLACE rows are emitted only after the corresponding whole-file CRC passes.
KEEP and directory rows can appear earlier; every row remains provisional until
the entire prefix/session verification, first filemark, tape-reader close and
checked output flush succeed. Later corruption, missing names, path conflicts,
read/status/close errors or output failure return `2`. Only a successful final
preview footer plus exit `0` confirms the complete preview. A partial action list
is not a successful plan. An output failure can return an error even after some
completion text has been delivered; require exit `0`.

The footer reports files that would be newly restored or replaced and checked
archive-directory entries. Planned replacements, selected files preserved and
filter-excluded files are reported separately. There is no “session restored”
message because no restore has occurred. No persistent plan or reservation is
created, and a later real restore must validate everything again.

To check existing file **contents**, use [`--compare`](archive-compare.md)
instead. Preview intentionally does not claim that a KEEP file matches archive data.

## Policy remains the same

`--dry-run` may appear once among options following the mandatory
`--restore --device UNIT --to TARGET` prefix. It combines with repeated
[`--only`/`--only-list`](selective-restore.md) and
[`--replace`/`--replace-list`](restore-selection.md). Syntax and all input lists
are checked before archive access or destination inspection. Invalid syntax,
empty filters, duplicates or replacement names outside an active filter return
`1`. The same path may appear in both roles; selection does not itself authorize
replacement, even in a preview.

The root and its parents must satisfy the same short-name DOS/local-path policy
as real restore. Without replacement permission the root must not exist; its
parent must already exist. Existing roots need the normal explicit replacement
selection. A parent that is a file, a directory/file collision or an existing
`OT$PART.TMP`/`OT$OLD.TMP` blocks preview just as it blocks normal restore.

Archive path, duplicate/prefix-collision, reserved-name and 256-entry limits,
all file CRCs and session counters are enforced. Every selected/filter name
must be seen as a regular file. Filtered preview suppresses directory rows and
excluded files but still verifies all excluded input. Planned parent directories
are checked without being created; a missing planned parent is not a false
“path not found” error for a new file below it.

Preview does **not** guarantee a later restore will succeed: it deliberately
does not test writing/metadata operations, writable capacity, filesystem write
protection or availability of a staging handle. It neither locks nor snapshots
the destination. Do not mutate the destination concurrently or use hostile
filesystem redirects. User-directed console redirection can create an output
file; that is separate from destination inspection.

## Tests

`make dos-preview-smoke` uses the already loaded, identity-guarded OA2101L8
virtual native archive. The large fixture has 95 records, 43 files and three
directories. The test previews all files into an absent root, then after
explicit rewind previews a filtered existing root with one authorized read-only
replacement, one selected existing file preserved and 41 excluded files.
It also checks malformed/duplicate flag syntax, empty filters, invalid tape
aliases, missing/file parents, existing root policy, leftover staging files,
unmatched archive names and an archive-directory/destination-file conflict.

The complete **writable** cloned DOS floppy image is saved before boot and must
remain byte-identical after all cases, including existing file bytes, attributes,
timestamps, directory entries and free space. This is stronger than checking
only the printed action list. Host and DOS restore/filter/exchange fixtures
still exercise the shared core. Physical devices, concurrency, disk-full/write
qualification and power-loss behavior remain outside this test's scope.

The memory-only QEMU archive fixture also exercises C in the real QUILIB DOS
screen, including cancellation and empty marks, native/legacy transport,
512-byte native blocks, an existing file preserved byte-for-byte, an invalid
file target rejected before rewind and the disabled action. Exact ASPI command
counts detect unintended I/O. The separate `preview-service` fixture also checks argument/filter validation,
unmatched paths and a successful quiet result; its entire writable floppy image
must remain byte-identical. `ui-preview-rewind-fault` and `ui-preview-read-fault`
check terminal failures without retry. No preview destination may be created.
Run `bash tests/dos_native_archive.sh ui ui-legacy fixed-ui ui-preview-keep
ui-preview-invalid ui-preview-disabled` (on one shell line); `MAXLANG=GR`
selects the German build. These fixtures never access a host tape device.


The DOS C action now uses the shared [read-side progress view](restore-progress.md).
Its file counter means planned files; no target writes or replacement grants
are introduced. Archive scanning, including excluded files, remains provisional
until complete verification, filemark and tape close succeed.

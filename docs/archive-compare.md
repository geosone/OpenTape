# Read-only archive-to-disk content comparison

```text
OPENTAPE --compare --device NRMT0 --to C:\RECOVER
OPENTAPE --compare --device NRMT0 --to C:\RECOVER --only-list FILES.LST
```

`--compare` checks existing destination files against a supported native archive
prefix using exact length, CRC32 and EOF. It does not restore or repair anything,
create directories/staging files, rename/delete files, or explicitly set
attributes/timestamps. Unlike [`--restore --dry-run`](restore-preview.md), it reads actual
destination file contents rather than only classifying planned actions.

The destination root must already exist and be a directory. No replacement
permission is needed: `--replace`, `--replace-list` and `--dry-run` are rejected
as syntax errors. Only the normal mandatory `--device UNIT --to TARGET` prefix
and optional repeated [`--only`/`--only-list`](selective-restore.md) filters are
accepted. Filter files use the shared checked list parser; malformed input,
duplicates and an empty combined filter fail before archive/destination access.

## What is compared

Every selected regular file must exist with the archive's exact length and
CRC32. Empty files must really be empty; trailing bytes fail. The comparator
reopens the file read-only with the existing source backend, denies cooperative
writers, verifies through a bounded 4 KiB buffer, requires exact EOF, rechecks
the source snapshot and checks close. It reuses the same
[source verifier as staged restore readback](restore-readback.md).

Without a filter, every explicit archive directory must exist as a directory,
including empty directories. With a filter, only selected regular files and
their existing parent directory paths are checked; unrelated archive-directory
entries are suppressed just as in filtered restore. Every filter name must be
seen as a regular file. Missing files/directories, file/directory conflicts,
unreadable files, mismatches or a missing selected name fail the command.

This is **content comparison**, not metadata equality or a mirror check.
Archived timestamps and attributes are not compared to current values; the
source snapshot only checks stability during its own read. Extra files or
directories outside the selected archive entries are ignored, never removed.
CRC32 checks corruption, not cryptographic authenticity or collision resistance.
The normal DOS/local short-path, regular-file snapshot, reserved staging-name,
archive-collision and 256-metadata-entry limits remain in force. A root containing
`OT$PART.TMP`/`OT$OLD.TMP` is refused; inspect previous incomplete work first.

## Archive verification and completion

The archive is still checked for framing, all data/record CRCs, whole-file CRCs
and session completion. Excluded file data is not compared to disk but remains
fully verified in the archive. A destination file is compared only after its
archive FILE_END passes whole-file verification. `MATCH Bytes ArchivePath` rows
then identify checked file lengths/CRCs; `DIR 0 ArchivePath` rows identify existing
directories. Rows remain provisional until the successful final footer and exit
`0`, because later input, a missing selection or tape close may still fail.

Comparison stops at the first input/destination error rather than accumulating
a complete difference report. There is no successful footer for an incomplete
comparison. Exit `1` is invalid syntax/device selection/filter preflight; exit
`2` is a comparison, destination, resource, archive or I/O failure. Completion
requires a valid closed session prefix, its first filemark, successful tape
close and checked console flush. A late console error may return failure even
after completion text has partly appeared; require exit `0`.

The command starts at the current tape position and does not rewind implicitly.
Use [`--rewind`](tape-positioning.md) explicitly for a BOT archive. Success is
not whole-volume verification, physical media/cache-bypass qualification, a
snapshot or a guarantee against later file changes. Do not mutate the destination
concurrently or use hostile filesystem redirects. User-directed console
redirection may create an output file separately from this read-only operation.
Filesystem-managed effects of reading files, such as access-time updates, depend
on the filesystem and are not controlled by OpenTape. The tested DOS floppy
images show no such changes and are checked byte for byte.

## DOS catalogue comparison

```text
OPENTAPE --native --ui --compare-to C:\RECOVER
```

Load the verified catalogue with L, mark regular files with Space (or A), then
press **K** to compare those files with the configured existing destination.
The **P** [path-settings editor](catalog-paths-ui.md) can also configure or
clear the comparison directory without starting an operation.
`--compare-to` is independent of `--preview-to` and `--restore-to`; omitting it
leaves K disabled until a comparison path is saved with P. The field accepts
at most 40 characters for the panel and uses the
same DOS local short-path validation. Missing, duplicate and overlong options
are rejected before device access.

The confirmation shows the selected alias, destination and selected-file count,
labels the operation read-only and limits the comparison to size/CRC/exact EOF,
with no metadata-equality claim. It explicitly requests **one rewind followed
by a fresh archive read**. Y confirms in English, J in German; any other key
cancels and is consumed. Input/filter validation and existing-root inspection
happen before movement. A failed rewind stops without a read or retry. Native,
legacy and explicitly configured native fixed-block transport are retained.

K copies only the extraction filter. Live O replacement intent is neither used
nor consumed, on success, failure, cancellation or a disabled action. Comparing
a file never grants replacement or authorizes a future W. The result returns to
the retained catalogue, marks and O flags when any key is pressed; that key is
consumed, so W at a result does not also enter a write dialog.

The quiet `ot_dos_compare_selected` service reuses the CLI restore/comparison
engine and backend, suppressing `MATCH` rows and console output for QUILIB.
It prepares no writable backend, creates no root/staging/recovery file and does
not modify bytes, attributes or timestamps. `ot_dos_restore_result.files` and
`readback_files` count successful content checks; `output_prepared`,
`destination_created` and `replaced` remain zero. Excluded archive files remain
verified, but their current disk files are ignored and may differ or be absent.
Every selected path must be present as an archive regular file.

The result shows matched/excluded counts and file-check diagnostics. Successful
size/CRC/EOF checks alone do not establish whole-batch success: full prefix
verification, selected-path completion, first filemark and successful tape close
are all required. Late archive errors retain provisional matched-file counts.
File-content/length mismatches, changed source snapshots and file read errors
are distinguished, with the full checked path shown across up to three rows
when available. The batch stops at the first error; it is not a full difference
report, metadata/mirror check or cryptographic comparison. The catalogue remains
an offline snapshot, not a guarantee of media identity.

The GUI reports the operation result in its panel; its later DOS exit code does
not summarize earlier comparison errors. Use the separate `--compare` CLI command for scripted
exit-status checks. Filesystem-managed reading effects remain subject to the
filesystem caveat above; DOS/FAT test images are checked byte-for-byte.

## Tests

`make dos-compare-smoke` first runs `dos-readback-smoke` to restore the existing
95-record OA2101L8 native fixture into a disposable DOS image. It then copies
that known tree to a fresh image and checks all 43 files/three directories plus
a two-file filtered selection. No tape data is written in either phase.

Negative fixtures alter an equal-length file, truncate another and append
trailing bytes. Other cases use a directory in place of a file, a missing file,
a missing archive directory, an absent root and an unmatched filter name.
Replacement/dry-run flags and empty filters are refused. Exactly two successful
comparison footers and 45 MATCH rows are expected across all cases; all three
content/length changes must be reported as mismatches. The complete writable
comparison floppy image must remain byte-identical to its pre-boot copy, proving
that even failed comparisons leave all fixtures untouched. Setup changes only
disposable image files, never physical destination data or the tape archive.


The DOS K action now displays [live archive/comparison progress](restore-progress.md)
through the same quiet comparison service. Matched-file counts follow successful
existing-file checks; no replacement authority or destination writes enter the
observer path, and a later archive/close failure remains a failed batch.

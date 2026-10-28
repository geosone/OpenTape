# Live progress for DOS restore, preview and comparison

The confirmed catalogue actions **W**, **C** and **K** now display progress while
reading and processing the archive. Their selected tape alias, backend/mode and
reviewed destination remain visible. The display shows:

- Reading/checking the archive, then waiting for filemark/tape close.
- Successfully restored, planned or compared files against the frozen selection.
- Kept and excluded files, separately from successful selected-file operations.
- Archive FILE_DATA bytes checked and processed so far.
- Current/last regular-file bytes and its complete archive path, across six rows.
- Whether that file belongs to the confirmed selection or is excluded.

Excluded file data is still verified. A sequential tape scan therefore may show
a path that was not selected, without creating that file on disk. Kept existing
files likewise contribute checked archive bytes without increasing the restored
file count. For totals beyond 4 GiB the display uses whole MiB, rounded down;
the current file's exact 32-bit byte count remains visible. These are archive
payload bytes, excluding framing, padding, directory metadata and disk readback
I/O. No whole-tape length, percentage, speed or remaining-time estimate is used.

Counts are based on the active read, not sizes remembered from an earlier
catalogue. The selected-file denominator is the immutable copied path count
from that batch's confirmation. Input counters include all processed FILE_DATA
records and remain cumulative across session headers. The current/last file's
byte count remains associated with its metadata until a new regular-file entry.

## Completion and failure boundaries

The core notifies only after a **complete OTB1 archive block** has passed
archive validation and every backend operation in that block has succeeded.
No notification advances a failed/short output write or failed commit. A packed
block can contain several records/files; it produces one notification with its
last active regular-file path and updated aggregate counters.

Archive validation precedes all output callbacks for that block. Backend effects
are not globally transactional: a failed block or later archive error can still
follow earlier file publication or partial staging. A missing progress event
therefore does not establish that no output exists. The ordinary result retains
partial-output/readback/recovery diagnostics and authoritative counters after
cleanup.

W counts a restored file only after its full backend commit succeeds, including
staged disk readback and the existing publication/replacement rules. Partial
side effects of a failed commit do not count as a completed file. C counts planned
files without writes. K counts files after the existing length/CRC/exact-EOF/
checked-close comparison succeeds. All three still verify excluded archive data.

Even when all selected files have completed, progress remains **provisional**.
SESSION_END changes the phase to waiting for the filemark and tape close; the
existing end-of-stream, selected-path coverage and close checks still run.
A later CRC, transport or close failure displays the ordinary failure result,
including any files already restored/read back. No progress callback reports
batch completion. Only the existing service result after cleanup can do so.

Busy progress has no active mouse controls or cancellation/retry behavior.
Result Back consumes its key/click as before. Replacement grants, new/existing
root rules, staging, readback, copied filters and one-shot confirmation are
unchanged. C/K retain live O intent; W consumes it only for the confirmed batch.
No additional tape movement, read, write or automatic recovery is introduced.

## APIs and ownership

`ot_restore_observe` attaches an optional observer before the first input block.
It receives a borrowed `const struct ot_restore` with committed block state,
current metadata, backend counters and cumulative checked-byte counters. The
core stays busy during notification; reentry, free and late observer changes
are refused. Observers may render/copy a snapshot but must not retain pointers,
mutate state, reenter the core or perform file/device I/O. There is no observer
callback from finish/free. Callers must still handle their normal returned error.

`ot_dos_selected_observed` selects the same preview/restore/comparison engines
as the existing quiet APIs, with optional observation. It validates action,
filter and replacement subset before I/O; comparison accepts no replacement
authority. Existing service wrappers and CLI calls use the same runner without
observers. `src/restore_progress_ui.c` renders through the main QUILIB line and
mouse-map infrastructure, using no independent transport or filesystem policy.
Settings profiles and archive formats are unchanged.

## Tests

```text
make OPENASPI=../OpenAspi dos-restore-progress-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-restore-progress-smoke
```

Six real DOS/QEMU scenarios cover restore, mouse restore, read-only preview,
comparison, excluded tree data and a late corrupt session boundary after one
file was published/read back. A test-only read delay starts at the third rewind,
after catalogue verification/loading, so captured VGA screens show genuine
intermediate states without changing production timing or adding tape commands.
Exact ASPI counts remain 37, 53 (tree) and 36 (late error). Source/output bytes
are checked, excluded output must stay absent, and preview/comparison require
whole-disk byte identity. The mouse case uses the [local CuteMouse fixture](dos-mouse.md).

Host and `RESTEST.EXE` checks observe successful blocks across source/backend/
CRC failures, skipped/excluded files, empty files and session boundaries. They
reject reentry/late registration and verify that a failed packed block can have
backend effects without publishing a progress snapshot or checked-byte advance.
Quiet DOS service tests cover invalid observed arguments without notification,
exact callback counts, partial data and checked-close completion. Host tests
also run under ASan/UBSan. No physical tape is used.

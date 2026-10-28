# Restore marked files from the DOS catalogue

```text
OPENTAPE --native --ui --restore-to C:\RECOVER
```

Load the verified catalogue with **L** after explicitly positioning the tape.
Space/Enter marks the current regular file; Enter on a directory toggles its
[regular-file descendants](catalog-directory-selection.md); A/N marks all/none. Press **W** to restore
those files to the configured destination. The confirmation names the tape
alias, destination and selected-file count and states that files will be written,
with a rewind followed by a fresh archive read. Y confirms in English, J in
German. Any other key cancels and is consumed, including W and Esc.

`--restore-to` initializes this action's destination. **P** in the
[path-settings editor](catalog-paths-ui.md) can configure it later; an empty
restore field keeps W disabled.
`--preview-to` may independently configure the [C preview](restore-preview.md).
Both use the existing DOS local short-path policy, with a 40-character UI limit.
Duplicate, missing or overlong startup options are rejected before device access.
Saving a destination performs no I/O and grants no write/replacement permission.

The confirmed batch now shows [live read/restore progress](restore-progress.md),
including the current full archive path, selected-file commits and excluded
archive scanning. Progress stays provisional through filemark and tape close.

## Selection and destination policy

W copies the marked regular-file paths through the same checked catalogue
conversion as C/export. Ambiguous versions and file/directory path collisions
are refused before any tape or destination operation. The copied filter is
immutable while confirmation and restore run. Marks do not grant replacement
rights. A directory Enter operation expands into explicit regular-file marks;
directory entries themselves are not copied into the extraction selection.

Without explicit replacement grants the destination root **must be new**.
An existing directory or file is refused, including a root created by an earlier
successful or partial W operation.

### Per-file replacement with O

After marking a regular file, press **O** on its row to toggle replacement
intent. `[x]` selects extraction only; `[!]` additionally names that exact file
for replacement. O on an unmarked file or directory does nothing. The header
shows both selected-file and replacement-grant counts. A selects all regular
files without granting replacement; deselecting a file clears its grant, and
N clears both kinds of intent. Navigation retains them without I/O.

C and W copy **two independent lists**: extraction paths and the subset with
explicit replacement grants. Both conversions reject ambiguous session/path
versions and roll back on failure. No wildcard, directory recursion or path
outside the extraction filter can broaden a grant. The confirmation displays
the number of replacement grants; a write confirmation with any grants explicitly
states that only `[!]` files may replace existing files.

A nonempty replacement list permits an existing destination directory. Existing
selected files without a grant are kept unchanged while their archived data is
still verified. A granted path absent from the destination is created normally.
Files outside the extraction filter are excluded but fully verified. The same
[per-file replacement policy](restore-selection.md) applies as in the CLI,
including read-only attributes, checked staging/readback, retained previous-file
recovery and checked publication. Existing files used as roots and leftover
`OT$PART.TMP`/`OT$OLD.TMP` are still refused during read-only preflight.

**Each confirmed W consumes the live replacement grants**, including on a
preflight, rewind, read or publication failure. Extraction marks remain. An
operation's frozen replacement list remains valid through its finish/cleanup;
it is then freed. Repeating W requires new explicit grants. Cancelling a dialog,
using C or [K content comparison](archive-compare.md#dos-catalogue-comparison),
or encountering a disabled W does not consume them. C shows planned
replacement counts but never writes or reserves anything. A previous preview
cannot authorize W on its own. Export E writes extraction paths only and carries
no replacement-grant metadata.

The runner allocates its buffer, validates both lists and inspects the destination
without writing before submitting the confirmed rewind. A failed rewind stops
without any read or directory creation. After successful tape open and mode
validation it checks the destination again and prepares the permitted root. Existing
parents must already exist. No competing program should mutate the destination
or tape during the operation; it does not lock/snapshot either one or establish
that the current medium is identical to the catalogue's original medium.

The selected native/legacy backend is retained, including explicitly configured
native fixed-block size. There is no automatic movement retry, mode guessing,
unload or changer operation. A fresh tape read must contain every selected path;
missing paths fail even if the remainder of the archive verifies.

## Writing, completion and errors

The shared [restore core](restore.md) verifies the entire supported prefix through
its first filemark, including excluded files. It creates parents only for selected
files. Each file uses exclusive staging, exact writes, archived length/CRC checks,
checked timestamp/close, then [staging readback](restore-readback.md) before
publication. These are the CLI's existing callbacks, not a second extraction
implementation. The GUI's service suppresses console output so it cannot corrupt
the QUILIB display.

The busy panel remains visible during this synchronous batch. The result reports
published file count, replaced and kept/excluded counts, and successful staging-readback count.
Success requires archive completion, all selected paths, successful file operations
and tape-reader close. A correctly published file alone does not imply that the
whole archive completed.

Failures retain the original operation result plus tape/DOS, disk, readback and
recovery diagnostics. If output creation started, the panel explicitly reports
that files or staging may remain. An incomplete archive can leave earlier fully
checked files published; there is no whole-session rollback. A failed current
staging file is closed once and may remain for inspection. Readback failures show
the complete checked path across up to three display rows. Never treat a failed
batch's counters as a complete restore result, or automatically rerun it into
its existing destination.

Any key returns from a result to the retained catalogue and marks. These remain
an offline snapshot. The UI's eventual DOS exit code does not summarize an
operation error previously shown in its panel. The CLI remains available for
scripting and replacement-list input.

## API and memory-only tests

`ot_dos_restore_selected` retains the new-root-only policy, and
`ot_dos_preview_selected` keeps all existing files. The explicit
`ot_dos_restore_replacing` / `ot_dos_preview_replacing` variants accept a separate,
optional replacement list and validate that it is a subset before any I/O.
They share `ot_dos_restore_result` and the existing CLI engine/callbacks.
`replaced` counts planned or fully published replacements. Preview counters are
provisional until the whole batch succeeds. The caller must confirm movement,
writes and replacement intent for every writable batch.

The preflight backend's `created` means prepared only; it is discarded before
initializing the writable backend in place. In the result, `destination_created`
means a new root was actually created, while `output_prepared` also covers a
reused existing root. Both are zero in preview. This distinction lets a read
failure after opening an existing root retain the partial-output warning without
claiming a new root was made. Backend diagnostics are captured after abort/close.

`tests/dos_native_archive.sh` boots a disposable DOS floppy with a resident
memory-only ASPI archive. The `ui-restore*` variants exercise the real QUILIB
screen: empty selection, consumed cancellation, disabled action, native/legacy
and fixed-block restore, filtered tree restore, refusal of existing/file targets,
failed rewind, hard read error and corruption of the final session record after
a file was published. Exact ASPI counts detect extra I/O and retries. Host checks
compare extracted bytes, confirm unmarked files were excluded, and require the
entire floppy image to remain byte-identical after cancellation, disabled action,
preflight refusal and failed rewind. No host tape device is accessed.

The `ui-replace*` variants add KEEP and replacement previews, native/legacy/fixed
replacement, a two-file tree with only one replacement grant, read-only originals,
consumed permissions, cancellation/disabled action, failed rewind/read, late
corruption and leftover recovery-file refusal. Unrelated files and ungranted
selected files must remain unchanged. Read-only/cancel/early-failure cases compare
the complete writable floppy image. `replace-service` runs the catalogue tests
under DOS and checks API subset/duplicate refusal, case-insensitive replacement,
existing-root and new-root results, readback and replacement counters.

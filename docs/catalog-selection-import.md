# Import a DOS catalogue file selection

```text
OPENTAPE --native --ui --selection-in PICKED.LST --restore-to C:\RECOVER
```

Load the verified catalogue with **L**, then press **I**. The confirmation shows
the configured input filename and warns that it replaces the extraction marks
and clears replacement grants. Y confirms in English, J in German; every other
key cancels and is consumed. The import does not read or reposition tape. It
uses the already verified, offline catalogue, whose enclosing archive read and
close must have succeeded before the view was opened.

`--selection-in` may appear once with the other `--ui` options. A missing, empty
or overlong filename argument is rejected before the UI/device is opened; the
panel limit is 40 characters. The option initializes the input field in the
[P path-settings editor](catalog-paths-ui.md), which can also configure it during
the session. An empty field leaves I disabled. The named file is opened only after
confirmation through the existing guarded DOS regular-file source backend.

## List format and resolution

The input uses the same bounded parser as CLI `--only-list`/`--replace-list` and
can be an [exported path list](catalog-selection-export.md). For example:

```text
# Exact archive-relative regular files only
data/file.bin
DATA/SUB/NOTE.TXT
```

See the [shared list syntax](restore-selection.md#parsing-and-authority): ASCII,
LF/CRLF, exact relative DOS 8.3 components, no wildcards, traversal, drive prefixes,
reserved staging names, duplicate names ignoring case or implicit recursion.
There may be at most 256 unique names and 128 KiB of input. Empty/comment-only
lists are refused; N remains the explicit way to clear all marks.

Every path must identify exactly one regular entry in the current catalogue,
matching ASCII case-insensitively. A missing name returns `OT_CATALOG_MISSING`
(`-164`). Reused paths across session/object versions, including file/directory
collisions, return `OT_CATALOG_AMBIGUOUS` (`-163`). A directory alone is not a
file selection and cannot select its descendants. Catalogue count/error limits
remain in force. The input cannot choose among versions by path alone.

## Atomic application and authority

The shared parser reads, validates, rechecks and closes the entire input before
any annotations are applied. All names are then resolved into a bounded bitmap;
only after every name resolves does the core replace the marks. File-open,
allocation, format/resource, source-change/close, missing-name, directory or
ambiguity errors preserve **all previous extraction marks and O grants**.
Failures do not poison the catalogue's capture error state, so a corrected list
can be retried. The panel shows the input filename, operation/DOS result and a
missing/ambiguous-name explanation where applicable.

Success selects exactly the listed regular entries and clears **every** O
replacement grant, even if the selected paths did not change. `[!]` grants are
not part of this file format and are never inferred from a filename. This is
selection input, not a replacement permission file. Use O afterwards to review
any exact replacement intent, then confirm the separate W batch.

The input's order/case does not reorder or rename catalogue entries. Inspection
cursor, page and path offset stay in place. Imported marks use the same C
preview, K content comparison, W restore and E export paths as manually marked
files; none starts automatically. Returning with B/Esc discards the catalogue
and its marks as before. Export E still writes only extraction paths and never
serializes O grants. A separately chosen CLI `--replace-list` is its own explicit
replacement authorization and must be reviewed for that purpose.

## API and tests

`ot_catalog_apply_selection` performs the bounded, atomic in-memory resolution
and keeps no pointers to the input list. `ot_dos_catalog_import` combines the
existing `ot_dos_source_open` and `ot_restore_selection_load` with this operation,
freeing its owned paths after every outcome. The input is read-only; there is
no new tape interface or list-parser implementation. GUI DOS exit status does
not summarize an earlier import error shown in the panel.

The memory-only QEMU `import-service` fixture runs the shared catalogue tests
under DOS and tests actual file loading, missing/malformed/duplicate/empty lists,
directory/unknown/version-ambiguous paths and rejection of a character-device
input without a SCSI submission. Host/DOS core tests cover 256 selected names,
independent input lifetime, case folding, late-resolution failure and
file/directory ambiguities in both orders, with unchanged prior marks/grants.

The real QUILIB `ui-import*` cases cover native/legacy/fixed-block catalogues,
input limits, failed/missing input, cancellation, disabled I, successful mark
replacement and explicit restore of just the imported file from a two-file tree.
All import-only cases require the **entire writable DOS floppy image** to remain
byte-identical. Exact ASPI counts prove that import adds no tape command. The
restore integration separately checks output bytes and excludes the previously
marked, unlisted file. No host tape device is used.

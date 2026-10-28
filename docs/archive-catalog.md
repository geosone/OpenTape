# Read-only DOS archive contents

In `OPENTAPE --ui`, press L to load contents from the selected raw alias's
current position. The confirmation shows the alias and first-filemark limit;
Y confirms in English, J in German. Other keys cancel without opening tape.
Rewind explicitly with R first when the archive label is at tape beginning.

The existing `ot_verify_session` and metadata visitor collect entries in tape
order. The list becomes visible only after full stream/file CRC/session checks,
the first filemark and successful close. A readable prefix or complete-looking
list followed by a close failure is not sufficient. Reading never rewinds,
writes tape, extracts files, creates a catalogue file or operates a changer.

The [batch interface](catalog-cli.md) supports `--list --catalog-out` and
offline `--list --catalog-in` with checked file ownership/publication.

Explicit [F8/F9 catalogue files](catalog-files.md) save metadata and load an
offline browsing view. Import does not establish tape identity or restore
authority.

L shows [live verification progress](verification-progress.md) while reading.
The catalogue becomes available only after complete verification and checked
close; even a complete-looking progress list is discarded on failure.

## Browsing

The list displays ten entries per page, with file/directory type, byte length
and a path preview. It includes empty directories. Up/Down select an entry;
PgUp/PgDn move ten entries; Home/End select the first/last entry. Navigation
clamps at the ends rather than wrapping. The detail row shows session/object
IDs and DOS attributes. The following row shows the selected path with its
zero-based character offset. Left/Right shift that detail window eight
characters, clamped to the path. Changing the selected entry resets the offset.
Long paths are retained completely (up to the format's 255-character limit),
not replaced by their clipped list previews.

B or Esc returns to the device screen and frees the entire list and its marks;
Esc does not also exit to DOS. Tape/device operation keys are ignored while
browsing, including unit numbers. Enter toggles the selected file or
[directory subtree](catalog-directory-selection.md). The selected row's yellow QUILIB
color means the current inspection row, independently of its mark.

## Temporary file marks

Space or Enter toggles the current regular file. Enter on a directory
selects/clears its regular-file descendants in the same session; the directory
itself remains unmarked. A marks all regular files and N clears
every mark; both letters accept either case. Directories display `[-]` and
cannot be marked, including through A. Files display `[ ]` or `[x]`; a marked
file with separate replacement intent displays `[!]`. The column heading shows
selected-file and replacement counts. O toggles the latter for a marked regular
file only; deselecting clears it and N clears both. A never grants replacement. Navigation and path inspection retain
marks without moving tape or changing the inspection cursor.

Marks annotate individual session/object entries, not path strings. Equal paths
in different sessions remain independent. They remain an in-memory
selection: E can now export them to a separately configured new path
list after confirmation. See [export semantics and safeguards](catalog-selection-export.md).
C now offers a [read-only restore preview](restore-preview.md#marked-files-in-the-dos-interface)
with a separately configured `--preview-to` destination and explicit rewind/read
confirmation. W offers [explicitly confirmed restore](catalog-restore.md)
with a separate `--restore-to` destination. A new root is required without O
grants. Confirmed W consumes replacement intent while retaining extraction
marks; C and cancelled dialogs retain both. K adds [read-only selected-file content comparison](archive-compare.md#dos-catalogue-comparison)
with `--compare-to`; it ignores and retains O intent. Enter selects a directory subtree within its
own session.
Marks do not modify
CLI `--only`/`--replace` lists or defaults. Returning with B/Esc, reloading,
changing devices or exiting cannot carry the marks into a later view/operation.

I now [imports an extraction selection](catalog-selection-import.md) from
`--selection-in FILE.LST`. Only a completely loaded and resolved list replaces
the marks. Success clears O grants; cancellation and failures retain them.

The live header identifies the selected alias and shows the total and page
range; its footer limits the claim to the first filemark. The view holds a
transient verified contents snapshot. Explicit F8 saves its metadata to a
[catalogue file](catalog-files.md); F9 loads a separately labelled offline view
with no mounted-tape identity or restore/replacement authority.
Leaving it does not undo consumed tape position. Another L starts a new read
at the current position; use a separately confirmed R when appropriate.

## Bounds and failures

`core/archive_catalog.c` copies transient visitor metadata into independently
allocated small linked nodes, avoiding a contiguous DOS allocation above 64 KiB.
The list has an explicit 256-entry total limit, including directories. It does
not scale without bound with tape length. Limit, allocation and invalid input
errors are sticky and stop the visitor; the session runner closes its handle.
The UI discards every node after any failed session/visitor/close attempt and
shows an incomplete diagnostic result, never an apparently verified partial
list. A failed attempt can still consume tape position. Freeing resets all list
state, so later reads cannot inherit old entries. Empty verified sessions can
display an empty list without index wrap.

The 256-entry limit is an initial DOS display bound, not an archive format or
CLI verifier limit. Larger catalogues, persistence, sorting, damaged-media
recovery, media locking and graphical restore selection remain unimplemented.

## Tests

Host/DOS streaming tests cover independent metadata copies, directories, exact
255-character paths, full 32-bit file sizes, indexed access, the exact 256-entry
boundary, sticky refusal, malformed/null input and reusable cleanup. Session
tests collect 30 entries on success and discard provisional entries after
corruption, early filemark, visitor overflow, fixed-mode refusal and failed
close. The host suite also runs under ASan/UBSan.
Marking tests exercise directory refusal without poisoning capture state,
idempotent set/clear, invalid index/value/arguments, independent equal-path
entries, mark-all/none including the 256-entry boundary, failed-catalogue
refusal and cleanup/reuse.

`make dos-ui-smoke` loads the approved virtual 95-record archive as 46 entries
(43 files and three directories). Real keyboard/VGA tests cover confirmation
cancellation, missing/custom aliases, ordinary-file rejection, five-page tail
handling, arrows, page/Home/End keys, path offsets, selected-row QUILIB colors,
ignored operation keys, B/Esc return, reload and no automatic rewind. The
same run now also checks Space toggle, all 43 regular files with A, N clearing,
unmarked directories, cross-page retention and an unmarked reload. The entire
writable cloned test image must remain byte-identical. Only the approved
virtual scratch cartridge is read/repositioned; no tape data is written.


The DOS contents view also supports [offline full-path search](catalog-search.md):
S edits a literal query and F3/Shift-F3 navigates hits in either direction with
wraparound. Search includes directories and repeated versions as separate entries;
it changes only the inspection cursor, preserving extraction/replacement marks.

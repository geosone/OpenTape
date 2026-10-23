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
browsing, including Enter and unit numbers. The selected row's yellow QUILIB
color means the current inspection row, independently of its mark.

## Temporary file marks

Space toggles the current regular file. A marks all regular files and N clears
every mark; both letters accept either case. Directories display `[-]` and
cannot be marked, including through A. Files display `[ ]` or `[x]`; the column
heading shows the marked-file count. Navigation and path inspection retain
marks without moving tape or changing the inspection cursor.

Marks annotate individual session/object entries, not path strings. Equal paths
in different sessions remain independent. They are currently an in-memory
preselection only: E can now export them to a separately configured new path
list after confirmation. See [export semantics and safeguards](catalog-selection-export.md).
There is no automatic restore, replacement authority or directory recursion.
Marks do not modify
CLI `--only`/`--replace` lists or defaults. Returning with B/Esc, reloading,
changing devices or exiting cannot carry the marks into a later view/operation.

The header identifies the selected alias and shows the total and page range;
the footer limits the claim to the first filemark. The screen is a transient
verified contents snapshot, not live media identity or a persistent catalogue.
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

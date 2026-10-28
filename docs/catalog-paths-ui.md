# Catalogue path settings in DOS

Start `OPENTAPE --ui` (or `OPENTAPE --native --ui`) and press **P** on the
device screen or in the verified catalogue. Five independent settings are
available, in this order:

| Field | Used by |
| --- | --- |
| Preview destination directory | C, read-only restore planning |
| Restore destination directory | W, confirmed file restoration |
| Comparison directory | K, read-only archive/file comparison |
| Selection input list | I, confirmed import of catalogue marks |
| New selection output list | E, confirmed export of marked paths |

Enter paths with the keyboard. Tab/Enter/Down moves forward, Shift-Tab/Up moves
backward. Home/End, arrows, selection, Backspace/Delete and insert/overwrite use
the existing QUILIB field editor. With a DOS mouse driver, click P, any field,
Save or Cancel. A field click positions its caret; clicking the blank tail
positions it at the text end. Save does not start the selected action.

**F2 saves all five fields together; Esc discards the entire draft.** Invalid
input keeps the editor open with a message and preserves the last saved values.
Every field has a 40-character limit; wildcards and control characters are
rejected. An **empty field disables only its corresponding action**. A later
P can configure it again. Normal DOS drive-qualified, absolute and relative
paths can be entered, subject to the existing operation's path policy.

F2 changes these session settings only. F5/F6 on the device screen can
explicitly [export/import a profile](settings-profiles.md) containing both
backup and path settings; startup does not automatically load one. The existing
`--preview-to`, `--restore-to`, `--compare-to`, `--selection-in` and
`--selection-out` options initialize these fields. Missing/empty/duplicate or
invalid option arguments are rejected before opening the UI. Starting without
these arguments leaves the corresponding fields empty until saved through P.

## Operation boundaries

The editor performs no filesystem or tape I/O. Saving does not check whether a
directory exists, create a directory/list file, open an input list, move tape
or change catalogue marks/replacement intent. The existing C/W/K/I/E services
perform their own full DOS path/type/existence checks only after their separate
confirmation. A syntactically accepted path is not a guarantee that the later
operation can use it.

P is available in an idle catalogue. At a pending confirmation P cancels that
confirmation and is consumed; another P is needed to open settings. At a file
operation result P dismisses that result. Settings cannot alter the frozen
selection or target during a confirmed operation. Saving leaves catalogue
cursor, marks and exact per-file O intent intact. It does not grant replacement:
W still reviews its destination and copied replacement subset, consumes that
batch's live grants only on confirmation and enforces staging/readback rules.
I clears replacement intent only after a successful actual list import.
E keeps its existing new-file-only policy; settings do not permit overwriting.

## Implementation and tests

`core/paths_form.c` owns bounded syntax checks and the complete draft commit.
`src/paths_dos_ui.c` renders the five-field view and handles F2/Esc. Both this
view and backup settings use `src/fields_dos_ui.c`, a small adapter around the
existing Open386MAX QUILIB FLDEDIT controls. It centralizes text navigation,
mouse focus, blank-tail clamping and the inherited overwrite-at-EOF bound.
QUILIB continues to own input, screen colors, cursor and mouse-driver handling.
No widget library or dependency source is copied or modified.

```text
make OPENASPI=../OpenAspi dos-paths-form-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-paths-form-smoke
```

The six fixtures cover saved/invalid/cancelled drafts, CLI presets, empty-field
disabling, marks/replacement preservation and confirmation boundaries. Real
file preview, comparison, restoration, list export and mouse-driven list
import/filtered restore run using paths entered in P without startup arguments.
File/list bytes, exact ASPI totals and applicable whole-disk non-mutation checks
are verified. `PATHTEST.EXE` also runs the portable transaction tests under DOS.
Mouse cases require the [local CuteMouse fixture](dos-mouse.md); all tape I/O
uses the memory-only ASPI fixture, with no physical cartridge access.

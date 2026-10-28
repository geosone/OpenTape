# Mouse operation in the DOS UI

OpenTape now uses QUILIB's existing mouse events as well as keyboard events.
Load an ordinary DOS INT 33h mouse driver (for example CuteMouse) before starting
OpenTape. QUILIB detects it through `init_input` and owns mouse polling, cursor
visibility and text-cell coordinates. No OpenTape mouse driver is installed.
With no mouse driver, the existing keyboard controls continue to work.

## Visible actions

Left-click the device-screen help labels for enumeration, status, rewind,
catalogue, backup settings, catalogue path settings, backup and native controls.
F7 opens read-only backup source preflight; its result Back control consumes
the click and returns without starting a backup.
F5/F6 on the device screen open explicit settings-profile export/import; their
filename field, Next, Confirm/Cancel and result Back controls use the same input
path. Importing settings cannot confirm a backup or restore. A click on an alias row selects that
alias; it does not automatically query it or perform a tape operation.

In the catalogue, click a text row to inspect it. Click its `[ ]`/`[x]`/`[!]`
checkbox to select/deselect a regular file; selecting a different row resets
path inspection to its beginning. Directory rows remain unmarked; select a
directory row, then click **Enter Mark/tree** to toggle its regular-file
[descendants](catalog-directory-selection.md). The same help action toggles
one file when a regular-file row is selected. O in the help
panel toggles replacement intent for the current marked file. The separate
bracketed A/N controls select all/none without implicitly granting replacement.
Bracketed arrow, page, home/end and path controls perform the existing keyboard
navigation operations.

In the F backup-settings editor, click a field to place its caret, click the
file/directory choice to set the mode, or use the separate F2 Save/Esc Cancel
buttons. Clicking beyond a field's text places the caret at its end. Saving
validates and commits session settings only; a later B still needs its own
write confirmation. Text entry remains on the keyboard.

P on the device screen or in the catalogue opens the five destination/list
fields. Click a field to place its caret and use the same Save/Cancel controls
as backup settings; text entry remains on the keyboard. Saving changes settings
only, retaining catalogue marks and replacement intent.

S in the catalogue opens the [full-path search](catalog-search.md) field.
Its Apply/Cancel controls accept or discard a draft; bracketed F3/Shift-F3
controls in the catalogue move to the next/previous hit. These actions only
change the inspection cursor and perform no disk/tape access.

The visible C/K/W/I/E labels open preview, comparison, restore, selection import
and export through the same handlers as keyboard commands. Disabled actions
retain their existing explanations. Result/back labels return from the current
view and consume that click. On the device screen B opens the configured backup
dialog; inside the catalogue B/Back returns to the device screen as before.

Confirmations have separate bracketed **Confirm** and **Cancel** controls in
the data panel. Only these modal controls are active until the dialog closes;
other menu/row clicks and blank-area clicks do nothing. Native positioning
editors have bracketed **Enter/Next** and **Cancel** controls, and the SPACE
chooser's visible rows can be clicked. Text/numeric entry still uses the
keyboard. Validation notices stay visible above the editing controls.

Mouse wheels, right/middle-button gestures and drag-selection are not assigned
commands. A mouse action does not bypass per-operation confirmation, replacement
policy, source validation, media checks or the one-batch backup write grant.

## Press/release and view boundaries

Actions run on **left-button release**, after a press inside the same visible
current-view target. Release without a matching press does nothing. Dragging
out of a target cancels the press, including if the pointer is brought back.
Keyboard input also cancels a pending mouse press.

Every redraw replaces the target map and drops a pending press. Busy views have
no active targets. The click that opens a dialog cannot also confirm it, and
an opener's repeated coordinates do not hit its separate confirmation control.
Result keys/clicks are consumed rather than replayed into a parent screen.
There is no held-button auto-repeat or automatic device/media operation.
Live backup and C/W/K read-side progress views have no active controls while I/O runs; the
separate result Back control retains its consumed-click behavior.

The shared `core/ui_mouse.c` mapper contains bounded, non-overlapping 80x25
text-cell targets and no device/UI-library I/O. `src/mouse_dos.c` translates
QUILIB `INPUTEVENT` press/release/drag events into the same existing command keys.
Application renderers bind only the current view; no label/path text is parsed
into an arbitrary command. Catalogue row tokens remain internal UI values.

## Tests

```text
make OPENASPI=../OpenAspi dos-mouse-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-mouse-smoke
```

The tests require QEMU/mtools, the existing SvarDOS fixture and a local CuteMouse
archive. `CTMOUSE_ZIP` can override the default
`$HOME/Iso/Dos/cutemouse21b4.zip`. The test copies the driver into disposable boot
images; it does not bundle it into the OpenTape release or install it on the host.

The real PS/2 path is exercised with CuteMouse, QEMU mouse movement/button
packets, QUILIB polling and captured VGA text. Test motion is split into bounded
packets and calibrated to the local CuteMouse /R00 driver settings. This avoids
PS/2 overflow/mouse acceleration artifacts; the OpenTape mouse adapter does not set
mouse sensitivity or mickey ratios.

`ui-mouse` checks menu/alias clicks, cancellation/blank clicks, confirmed rewind,
catalogue row/checkbox marking, O/A/N, navigation and path scrolling, preview,
disabled actions, native load/EOD cancellation, numeric-editor Next/Cancel and
backup/back/exit. Its complete DOS disk image remains byte-identical and exact
ASPI totals detect extra device commands.

`ui-backup-mouse` checks separate opener/confirmation clicks, drag-out cancellation,
write consent, result dismissal and a fresh confirmation for another attempt.
An independent rewind/comparison verifies the memory-only written tape; the
entire DOS disk image remains unchanged. `ui-import-mouse` cancels then imports
a list, checks that old replacement intent is cleared, confirms restore of only
the imported file and compares output bytes while requiring the unlisted file
to remain absent. No physical tape is used.

The portable mapper tests also run under DOS and host ASan/UBSan: rectangle bounds,
overlap/capacity, edge hits, stray/repeated releases, changed views, keyboard
cancellation and drag-out/back behavior. Existing keyboard flows and the native
controls' QUILIB colors remain regression-tested.

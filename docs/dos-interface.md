# DOS device and status screen

```text
OPENTAPE
OPENTAPE --ui
OPENTAPE --ui --device NRST0$
OPENTAPE --ui --selection-out C:\LISTS\PICKED.LST
```

OpenTape now opens an interactive device/status screen instead of the previous
press-any-key placeholder. The default selected alias is NRMT0. `--ui --device`
accepts a validated raw no-rewind alias, including a configured custom letter
and optional `$`; it normalizes ASCII case. Drive/path names, buffered aliases
and rewind-on-close names are refused before entering the screen.

## Keys

| Key | Action |
| --- | --- |
| D | Refresh the four aliases in the current namespace. |
| 0–3 | Explicitly select that unit number, preserving letter and `$`. |
| Up/Down | Select previous/next unit, wrapping between 0 and 3. |
| S or Enter | Query the selected alias's status. |
| R | Ask to rewind the selected alias; does not move tape yet. |
| V | Ask to verify the archive from the selected alias's current position. |
| L | Ask to load verified archive contents into a read-only paged view. |
| Esc | Close the input interface, restore the cursor and return to DOS. |

Other keys and mouse-only events are ignored. Selection itself does not probe
hardware. Startup does not enumerate or query anything automatically. Refresh
does not select a FOUND alias or change the current selection. Both selecting
an alias and refreshing the list invalidate the separate status panel, avoiding
an old unit's status under a new selection. Press S/Enter for a new snapshot.
Selection lasts only for the current screen session; it does not change the
defaults of later CLI commands or rewrite driver configuration.

## Confirmed rewind

R shows the exact selected alias and asks for confirmation. Only Y in English
or J in German (either case) confirms. Every other key cancels and is consumed:
Enter does not confirm, a number does not switch devices, and Esc cancels the
question without exiting the screen. Press Esc again to exit. There is no
automatic retry or remembered confirmation.

Confirmation calls the existing synchronous `ot_dos_tape_rewind` path, with
the same raw-alias, character-device, mode, status and close checks as the CLI.
The screen shows a waiting message while DOS blocks on the operation. It does
not provide progress, timeout or cancellation of an in-flight device command.
Success or result/DOS error is shown afterward. Every confirmed attempt clears
cached enumeration and status, including failed attempts; query S or D again
for a fresh snapshot. Cancellation leaves the selection and snapshots intact.
Moving the tape changes the starting position of subsequent CLI operations.
A failure does not prove the tape stayed in place (for example, close or
post-command status can fail after movement). Inspect the drive before retrying;
the UI never retries automatically. See [rewind limits](tape-positioning.md).

## Confirmed archive verification

V shows the exact alias, current-position warning and first-filemark scope.
Only Y in English or J in German (either case) starts reading. Every other key
cancels and is consumed, including Enter, Esc, device numbers and R. Cancellation
leaves selection and snapshots intact and does not open a device. To start a
standalone native archive at its label, explicitly rewind with R beforehand.

The UI and CLI `--verify --all`/`--list` share `ot_verify_session`, which opens
the raw alias read-only, runs the existing strict stream verifier to the first
filemark and closes once. There is no automatic rewind, read retry, extraction,
catalogue allocation or file output. The only archive-size-independent input
buffer is the same 32 KiB buffer used by the CLI. Allocation failure is reported
before opening a device. `--native --fixed-block N --ui` additionally uses
checked fixed-block archive reassembly and displays the selected physical size;
the ordinary legacy path remains variable-record only.

Confirmed attempts clear cached enumeration and status. The screen displays a
waiting message while synchronous verification runs; there is no progress meter,
application timeout or in-flight cancellation. Driver timeouts still apply.
Success requires both complete file/session CRC verification and successful
close, not just readable framing. The result panel shows accepted blocks/records,
closed sessions, and the explicit first-filemark/not-whole-volume limit. On
failure it shows result/DOS errors and marks counters diagnostic only, never a
successful partial archive. A second verification starts wherever the first one
stopped; no successful result persists under a different selection or operation.
Ignored keys leave the result visible. Query S/D again for a current snapshot.

Reading consumes tape position even when the verifier rejects a block or close
fails. Run no concurrent tape application. The normal Esc exit code does not
encode a previously displayed operation failure. See
[verification semantics and limitations](archive-verification.md).

## Verified contents view

L uses the same confirmation and verified session runner, adding a bounded
metadata collector. Only successful verification and close open the view.
Up/Down, PgUp/PgDn and Home/End navigate; Left/Right inspect the complete
selected path. Space toggles a temporary file mark, A marks all regular files
and N clears them; directories cannot be marked. B/Esc return to the device
screen and discard both list and marks. Marks grant no restore/replacement
authority and other device keys are ignored inside the view.
With an explicit `--selection-out` target, E confirms export of marked paths
through the existing protected writer. No target means no export; existing
files are never overwritten. See [export controls and limits](catalog-selection-export.md).
The initial limit is 256 total entries; any overflow/error discards the whole
provisional list. See [catalogue bounds, controls and tests](archive-catalog.md).

## Display and safety

The main panel shows the explicitly selected alias, four cached enumeration
rows, a refresh number and scan/error counts, followed by the requested status.
Rows retain the shared FOUND/MISSING/REJECTED/ERROR meaning described in
[alias enumeration](device-enumeration.md). FOUND is not a readiness guarantee.
The status section displays ASPI address, state, block size, ABI, extended driver
error when available, and last SCSI/sense/opcode/residual values. Legacy status
uses `driver=--`; a failed query shows result/DOS errors rather than stale fields.

The footer labels the configured **status path**, not an unperformed hardware
detection or an automatically selected working drive. Cached snapshots are not
live monitoring. Run no competing tape application and retain each subsequent
operation's own media/transport validation.

Status and enumeration use the existing status-only interfaces. Explicitly
confirmed rewind is the only media-control request; confirmed verification reads
tape data and advances position. The screen contains no spacing, unload, write
or changer action. Only explicitly configured and confirmed E export creates
a new disk path-list file; it never rewrites configuration or grants
write/replacement authority. Backup,
archive file selection, restore/preview/compare dialogs and writable workflows
remain CLI-only; this is the first usable screen, not a complete backup TUI.

Rendering and event handling come directly from Open386MAX's QUILIB:
`initializeScreenInfo`, `paintBackgroundScreen`, shared windows/colors,
`get_input`, `end_input` and cursor/window functions. No private widget, mouse,
keyboard, palette or window framework is added. The blue data area, yellow
selection and gray help area reuse `DataColor`, `DataYellowColor` and
`QuickBackColor`. The row helper bounds the QUILIB copy by both window width
and actual text length; QUILIB's fixed-count writer is not treated as a
NUL-terminated string printer. Labels/help are English or German via MAXLANG.

## Tests

`make dos-ui-smoke` drives real keyboard events in the DOS/mhVTL VM and captures
80x25 VGA text memory and its attributes, without adding a production test hook.
Checkpoints cover initial unqueried state, enumeration/rejected lookalikes,
explicit selection, missing/regular-file status failures, stale-status removal,
refresh without auto-selection, Enter status, ignored keys, arrow wrap, Esc,
custom letter/dollar preservation and startup without arguments.
Rewind checkpoints additionally cover Enter/Esc/number cancellation without
changing selection, missing/ordinary-file refusal, uppercase confirmation,
snapshot invalidation and status re-query after a successful rewind.
Verification checkpoints cover confirmation cancellation, missing/custom aliases,
ordinary-file rejection, full 95-record/one-session success, retained results on
ignored keys, and a second attempt without rewind refusing to replay the session.
Another explicit UI rewind restores the label for final CLI verification.

The test checks the actual shared VGA colors: selected data `1Eh`, ordinary
data `1Fh` and help text `7Fh`. A subsequent CLI verification must accept all
95 records from the unchanged tape beginning after the cancelled/refused
operations. A fourth UI session rewinds the now-consumed session explicitly;
a confirmed UI verification reads all 95 records and a repeated attempt must
fail without an implicit rewind. After a second explicit UI rewind, another
CLI verification must accept the same 95 records from its beginning.
The complete writable cloned floppy must remain byte-identical before/after
all screen sessions. Artifacts are under
`build/{US,GR}/dos-tape-tree-ui-large/ui-*.{bin,txt}`. The harness positions only
the approved OA2101L8 virtual fixture before boot. No tape data is written;
physical displays, mouse workflows and the full backup TUI remain unqualified.
The same run now also browses/reloads all 46 verified catalogue entries,
checks navigation/detail offsets and B/Esc return, rejects rereading without
rewind, and explicitly rewinds again for the final CLI verification.

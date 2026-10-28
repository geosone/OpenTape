# Native tape controls in the DOS interface

Start `OPENTAPE --native --ui`, select alias `0..3`, then press **T** for tape
controls. The subview uses the application's existing QUILIB windows, colors
and input lifecycle. Its selected alias stays fixed until returning to the
device screen; digits in a target field cannot select another drive.

| Key | Action |
| --- | --- |
| P | Read the long 64-bit logical position and logical filemark identifier |
| I | Read reported partition metadata, with pending-layout caveat |
| S | Choose relative record/filemark spacing or end of data |
| A | Explicitly retrieve TapeAlert after acknowledgment |
| G | Enter a logical target and optional explicit partition |
| L / U | Load / unload the selected tape |
| N | Retension: LOAD with RETEN, own capability bit, no BOT claim |
| K / O | Prevent / allow cartridge removal |
| R | Explicit synchronous rewind |
| ESC | Cancel editing/confirmation, or return to the device screen |

Position and layout observation are immediate and read-only. Every movement or removal
command has a separate confirmation displaying the selected device and action.
LOCATE additionally displays the complete address and partition policy before
confirmation. English uses **Y**, German **J**; any other key cancels and is
consumed without becoming a different command. Rewind affects the selected
partition and does not force partition zero.

## Target input

The logical object field accepts decimal `0..18446744073709551615`. Enter
validates the entire value before advancing. The partition field accepts
`0..255`; an empty field means the current partition. Explicit `0` selects
partition zero and is not equivalent to an empty field. These are native tape
object coordinates, not byte offsets, archive filenames or restore checkpoints.

Backspace corrects input. ESC abandons it without opening a device. Empty or
out-of-range object input, invalid partition numbers and nonnumeric printable
characters cannot reach confirmation. Exceeding the field width latches an
input error: ignored excess digits cannot silently turn into a shorter valid
target. Correct with backspace or cancel. Twenty-digit values have their own
row and remain fully visible in the narrow data window.

See the [architecture](architecture.md) for backend boundaries and the
[roadmap](roadmap.md) for remaining work.

## Relative positioning

Press **S**, then **1** for records, **2** for filemarks or **3** for end of
data. Matching SDK headers also offer **4** for consecutive filemarks,
**5** for setmarks and **6** for consecutive setmarks. Consecutive spacing
searches for a run of N adjacent matching marks, not N separate runs. Setmarks
are distinct from filemarks; required drive modes are not enabled implicitly.
Counted operations accept a signed decimal count from
`-8388608` to `8388607`; negative counts request backward movement, positive
counts forward movement. Zero is an explicit `SPACE 0` request, not a canceled
command. These counts describe tape objects, not bytes, filesystem files or
absolute block addresses. EOD has no count editor and always sends count zero.
SPACE uses the current partition and does not switch partitions.
The confirmation displays the selected device, operation, complete count and
direction. It uses **Y** in English and **J** in German. Every other key cancels
and is consumed; it cannot invoke another command. ESC also cancels the choice
and count editors without opening the device. Other keys at the unit chooser
cannot select another device or invoke a control.

A minus sign is accepted only at the start. Empty input, a lone minus,
out-of-range values and invalid characters cannot reach confirmation. Extra
characters latch an input error until backspace/cancel, so truncation cannot
silently change the requested count. CLI and UI use the same bounded parser;
invalid parsing preserves its output and performs no I/O.

Confirmed movement calls `ot_dos_native_space` exactly once. The existing SDK
checks provider capability, exclusive ownership and clean native session before
submission. A failed move reports unknown position and retained uncertainty;
subsequent SPACE requests remain blocked until explicit successful rewind.
Reading a position does not recover the session. There is no automatic retry,
rewind or emulation, and reported command completion is not a BOT claim.
Older SDK headers that already provide basic SPACE can build this view even without
wide positioning, removal controls or partition queries. Entries 4/5/6 then
report an unavailable SDK feature without I/O.

## TapeAlert

**A** shows the selected device and a notice that reading may acknowledge
drive warning flags. Only Y/J confirms. Other keys cancel and are consumed,
including P; they cannot become a position query or another action. The view
reports the number of returned flag states and active flags, followed by active
numeric codes. All 64 codes fit in the existing QUILIB window without clipping.

Missing flag states remain unknown. A parsed empty page is not an all-clear
health verdict, and zero values do not prove that each diagnostic is supported.
Unverified/malformed/failed replies show no flag list. Native uncertainty and
diagnostics remain visible. The notice explains the read side effect; no
LOG SELECT, automatic polling, retry or persistent warning history is added.
Older SDK headers show the missing-feature message without a command.

## Results and ownership

The view calls the same checked `ot_dos_native_*` SDK functions as the CLI.
Editing, confirmation, cancellation and rendering submit no SCSI commands.
There is no automatic readiness probe, position refresh, retry, rewind, mark,
flush or movement fallback. Synchronous operations display a busy message
before submission, then retain their result and available diagnostics.

Position validity remains explicit. Unknown object positions are not displayed
as block zero; the logical filemark identifier has independent validity. Legacy
managers without transfer-length proof display an unverified-length message.
A valid position cannot clear an earlier native failure. Class, host/target,
key and available sense details remain visible after command errors.

An active writer or uncertain native state can block LOCATE. Explicit removal
controls may complete while earlier uncertainty remains, so the completion
message and retained session warning are separate. Successful rewind remains
the explicit native recovery operation. Prevented UNLOAD still reports failed
movement and uncertainty; it does not automatically unlock or retry. An error
reading an empty drive does not invent a new native writer failure.

If the hardware action completed but close failed, the view distinguishes
those facts instead of reporting that the action was never performed. Invalid
or missing reply packets supply no invented clean session or completion facts.
Old provider-header builds keep the subview available for their supported
controls and explicitly reject unavailable wide positioning/removal controls.
There is no fallback to READY or a legacy movement command.

Returning to the device screen invalidates UI snapshots and cached catalogue
entries; the user explicitly queries or loads them again. The driver's session
state is not reset. The legacy UI keeps its existing keys and does not expose
the native-controls entry.

## Implementation and qualification

`src/native_dos_ui.c` owns the bounded dialog state machine; its public adapter
is `include/opentape/native_ui.h`. `main.c` supplies the established window-line
renderer and owns screen/input setup and cleanup. The DOS link explicitly
selects Open386MAX's existing QUILIB `OWNER.OBJ`: the historical editor also
exports `QuickWin`, so leaving its owner to archive lookup could accidentally
pull unrelated editor/help modules. No QUILIB source or GDI installer fork is
introduced.

`tests/dos_native_controls.sh` runs disposable DOS images and drives real
keyboard events through QEMU's monitor. It captures VGA text/attributes rather
than using a production test hook. Exact command counters prove that invalid
input, canceled commands, busy sessions, rejected ordinary files and missing
aliases cause no unexpected SCSI submissions. The complete normal workflow
covers full-width positioning, explicit/current partition, locks, prevented
UNLOAD, retained uncertainty, recovery, empty drive, reload and retension.

Fault cases cover host/deferred/unsupported movement errors, a changed cursor
after failure, position observation without recovery and busy rejection until
explicit rewind. Unknown and unverified responses, old SDK headers, both
languages, complete help labels and QUILIB color consistency are tested.
No host tape is attached. Real drives and Windows interfaces remain separate
qualification work.

The **I** view displays maximum/defined additional partition counts, raw flags,
recognition/type/units and descriptor count. These may be pending or stale;
they never restrict or authorize the **G** target editor. Failed, malformed and
unverified observations expose no layout values. Format/partition creation and
committed-layout identification remain separate work.

`make OPENASPI=../OpenAspi dos-native-space-ui-smoke` runs the signed SPACE
GUI cases; `MAXLANG=GR` selects German. The existing memory-only manager checks
all eight CDBs independently: records +1/-1/max/min/0, filemarks +2/-2 and EOD.
The tests also cover canceled/invalid input, retained failures, busy rejection,
explicit recovery, rejected files/missing aliases and actual VGA attributes.

`tests/dos_native_controls.sh space-ext-normal` verifies 15 CDBs independently:
all three additional mark modes with +1/-1/max/min/0. `space-ext-host`,
`space-ext-unsupported`, `space-ext-deferred`, `space-ext-contradict`,
`space-ext-mark` and `space-ext-descriptor` exercise submitted failures,
observation without recovery, busy gates and explicit rewind. `space-ext-legacy`
uses a manager without residual support. `MAXLANG=GR` selects German.

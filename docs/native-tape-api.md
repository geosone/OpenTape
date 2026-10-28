# Native tape API: first DOS operations

OpenTape now consumes the native OpenASPI service for query, readiness and
synchronous rewind through a shared adapter. A persistent native reader now
handles variable-length records and explicit stream conditions. A persistent
writer implements explicit finalization. [Archive commands](native-archive.md)
can use both through `--native`, including explicit fixed-block archives via
`--native --fixed-block N`. Windows bindings remain open.

```text
OPENTAPE --native-status [--refresh] [--device NRMT0]
OPENTAPE --native-ready [--device NRMT0]
OPENTAPE --native-rewind [--device NRMT0]
```

Snapshot status sends no SCSI commands. `--refresh` requests bounded read-only
identity, limits and mode-page probes. Missing information remains unknown;
partial results retain diagnostics. A legacy manager without a verified reply
byte count is identified in the output even after structural checks pass.
READY checks readiness once. REWIND explicitly moves the tape and waits for
completion; success confirms beginning of tape. Failed rewind reports unknown
position and is never automatically retried.

Only raw no-rewind names (`NRMT0` etc.) are accepted. Invalid syntax is rejected
before opening. An ordinary file with such a name is rejected as a non-device
and is not modified. Commands do not silently fall back to the legacy API.
Exit codes are 0 for complete success, 1 for invalid arguments, and 2 for a
partial probe, busy session, transport/target failure or platform error.
English and German messages follow `MAXLANG=US` / `MAXLANG=GR`.

## Shared adapter and DOS binding

`include/opentape/native_tape.h` and `core/tape_native_api.c` define synchronous
callback-based operations. Platform handles live in the callback context;
the interface does not encode DOS descriptors or NT pointers. Identifier
validation belongs to each provider. Responses are checked for size/version,
field consistency, echoed request and guard corruption before being exposed.
Each successful open receives exactly one close attempt. The first failure
is retained. A close failure following a completed rewind preserves the valid
command result, so callers can distinguish completed movement from close
failure and avoid replaying it.

Controls check a snapshot and the advertised operation mask on the same open
handle before submitting. Written/buffered sessions and retained write errors
block control. REWIND is also blocked while another rewind is pending; READY
may inspect that pending operation. The provider repeats its ownership checks
when handling the control request.

The DOS adapter reuses existing handle/device/IOCTL helpers. Its native packet
contract is documented in OpenASPI's `docs/native-tape-query.md` and
`docs/native-tape-control.md`. Legacy ASPITAPE remains a separate provider.

## Building and tests

Native commands are enabled when the selected OpenASPI checkout contains
`include/openaspi/tape_api.h`. The older pinned submodule still builds without
them. Until a matching OpenASPI revision is committed and pinned, use the live
checkout explicitly and a separate output directory:

```sh
make OPENASPI=../OpenAspi native-api-test
make -f dos.mk OPENASPI=../OpenAspi MAXLANG=US \
  OUT="$PWD/build/native-us-obj" BINDIR="$PWD/build/native-us" \
  "$PWD/build/native-us/OPENTAPE.EXE"
bash tests/dos_native_api.sh
MAXLANG=GR bash tests/dos_native_api.sh normal partial control-target
```

The DOS test defaults to the sibling `../OpenAspi` and the local SvarDOS floppy
ZIP; `OPENASPI`, `SVARDOS_ZIP` and `OUT` can override those locations. It runs
the actual CLI and shared C tests against a memory-only ASPI manager, checks
exact command counts and protects the ordinary-file rejection fixture.
Host tests cover lifecycle, malformed responses, session gates and error
precedence. No physical tape is accessed by these tests.

## Persistent native reader

`ot_native_reader_open/read/close` keeps the same handle/context through the
record stream. DOS uses `ot_dos_native_reader_*` and marshals buffer pointers
inside the adapter. Zero-initialize a reader before first open and keep its
callback context alive until close. Read does not open, query or close again
for each record. A malformed request is rejected before submission; a failed
read latches the reader, and further reads submit nothing until explicit close.

`OT_NATIVE_OK` means a completed ordinary record. `OT_NATIVE_STREAM` means a
completed read with FILEMARK/SETMARK/EOM/ILI/EOD: inspect `reader.last` for both actual
bytes and stream flags. FILEMARK may accompany data in the same result, without
an extra synthetic EOF response. A zero-length ordinary record is not a
filemark. Actual bytes are usable only with `OT_TAPE_COUNT_VALID`. A negative
result remains an error even when a valid partial byte count accompanies it.
Do not discard diagnostics or automatically replay an ambiguous read.

Driver-detected fixed-only devices currently return UNSUPPORTED without a READ
command. This entry always requests variable records and does not change the
drive mode; callers must select a drive configured accordingly. Native
record writes use the separate explicit writer contract below.

`tests/dos_native_api.sh` includes `NTREAD.EXE`, which exercises the installed
provider through the real DOS adapter. Fixtures check full/short/empty records,
filemarks with/without bytes, EOM/EOD, host/deferred failures, invalid/conflicting
counts, truncation, partial hard errors, fixed-mode rejection and old-manager
sense counts. Exact command counts prove that failed readers do not replay.

## Persistent native writer

`ot_native_writer_open/write/flush/filemark/finish/abort/close` supplies an open
session with a distinct write-open callback. DOS uses `ot_dos_native_writer_*`
and opens an existing device with `O_RDWR`, never CREATE. It rejects ordinary
files before BEGIN or data/mark submission. The operation mask must advertise
WRITE and every writer control before ownership is claimed.

BEGIN claims without hardware I/O. WRITE submits one variable record. FLUSH
sends synchronous WRITE FILEMARKS with count zero; FILEMARK explicitly sends
count one. FINISH sends count zero and releases the claim after successful
completion. It neither appends an archive delimiter nor backspaces. CLOSE
only aborts any remaining claim and closes the handle once; an unfinished or
aborted stream returns `OT_NATIVE_UNFINISHED`, even if DOS CLOSE succeeds.
Applications must request their own desired filemarks and FINISH explicitly.

A short write fails with a valid partial count, rather than sending a remainder
as a second record. Target/host/transport errors, EOM and failed finalization
latch the writer; subsequent write/flush/mark/end calls submit nothing. The
first error and its validated packet survive abort and cleanup. Malformed
responses are not exposed as successful packet data.

Native session flags `0010h` (active), `0020h` (dirty) and `0040h` (failed or
abandoned) are available in snapshots. DOS can ignore driver OPEN/CLOSE
errors: an extra handle poisons the claim, and an abrupt close retains failed
state without writing marks. After releasing a dirty/failed writer, an explicit
successful native REWIND is required before reuse; READY does not repair it.
This claim guards ASPITAP entry points; direct ASPI clients must cooperate in
device ownership. See OpenASPI's `docs/native-tape-write.md` for the wire rules.

`NTWRITE.EXE` runs the actual DOS writer against the memory-only provider.
Fixtures cover full/short writes, write protection, partial/full EOM,
host/deferred/contradictory completions, malformed residuals, partial hard
errors, old managers, fixed-only rejection and each failed finalization stage.
They verify exact command counts, preserved failures, file rejection and
unfinished/abrupt closes. Archive commands now connect through the common native bridge; see
[native-archive.md](native-archive.md) for selection, EOF and finalization policy.

Drive-specific cases are reserved for a later separate quirks layer. The native
archive bridge implements common record and completion semantics without
vendor-name branches or speculative command alternatives.

## Explicit fixed-block SDK sessions

`ot_native_reader_open_fixed` / `ot_native_writer_open_fixed` and the DOS
`ot_dos_native_*_open_fixed` wrappers select a block size when opening. The
provider must advertise `OT_TAPE_OP_FIXED_IO` (`0400h`) before a fixed session
is used. This is API support; the drive must already be configured for that
size. These calls issue no MODE SELECT or implicit media preparation.

Existing read/write functions then accept whole multiples of that block size.
All application counts remain bytes; the provider encodes SCSI CDB block counts
and validates aligned ASPI byte residuals and bounded sense-block conversion.
Mode/size echoes are validated, invalid lengths submit nothing, and partial
errors retain data/count diagnostics without record replay. Fixed writer claims
keep their data mode through the ordinary flush/filemark/end/abort controls.

Record `block_size` is a DWORD at offset `2Ch`; fixed BEGIN uses a DWORD at
`1Ch` in the control packet. Their sizes remain 64 and version `0100h`; flag
`OT_TAPE_IO_FIXED` gates these formerly reserved fields. Variable requests keep
flag and size zero. SDK tests check negotiation against older providers, missing
capabilities, alignment, echoed mode and failed-session gates. `NTFIX.EXE` checks
the actual DOS provider/consumer with two 512-byte blocks and exact command
counts, including old managers, filemarks, EOM, ILI and invalid residuals.

The archive bridge now offers explicit fixed-mode attachments with separate
caller-owned workspace. It preserves OTB1 logical bytes, aligns each block to
physical boundaries with checked zero padding and reconstructs complete logical
blocks before verification/restore. Ordinary attachments without workspace still
reject fixed sessions. See [fixed archive commands](native-archive.md#fixed-block-archives)
for `--native --fixed-block N`, framing and limits.
See OpenASPI's `docs/native-tape-fixed.md` for the full wire and count contract.


## Relative positioning and EOD

`ot_native_space` and `ot_dos_native_space` add signed record/filemark movement
and EOD using CONTROL operations 7..9; operations 14..16 add consecutive
filemarks, setmarks and consecutive setmarks. A conditional count DWORD occupies
packet offset `20h`, leaving the size at 64 and version at `0100h`. Provider
bits `0800h`/`1000h`/`2000h` are checked individually; older providers submit
no movement. Replies must echo the count, including BUSY/error replies.

```dos
OPENTAPE --native-space-records 10 --device NRMT0
OPENTAPE --native-space-records -1 --device NRMT0
OPENTAPE --native-space-filemarks -2 --device NRMT0
OPENTAPE --native-eod --device NRMT0
```

Decimal counts are -8388608..8388607; EOD takes no count. Counts describe
physical tape records/blocks or filemarks, not archive files or logical OTB1
blocks. The drive may reject optional reverse movement. No operation changes
block mode or retries/falls back to another command.

A clean move completes with SUBMITTED|DONE, never an invented BOT/absolute
coordinate. A submitted failure keeps POSITION_UNKNOWN and native FAILED
through close/reopen; data I/O and more movement remain blocked. Status/READY
can inspect this state but cannot repair it. Release any writer and perform a
successful explicit native REWIND before reuse. Native session warnings cover
failed movement as well as failed/abandoned writing.

`make OPENASPI=../OpenAspi dos-native-space-smoke` runs the actual DOS provider,
shared SDK and CLI with exact command-count/CDB assertions on memory-only
fixtures. Host tests check signed bounds, individual capability bits, malformed
replies and first-error cleanup; the harness also runs in German. Physical
movement and optional-device behavior remain to be qualified.

## Synchronous native LOAD/UNLOAD

```text
OPENTAPE --native-unload --device NRMT0
OPENTAPE --native-load --device NRMT0
OPENTAPE --native-ready --device NRMT0
OPENTAPE --native-rewind --device NRMT0
```

`ot_native_execute` and its DOS wrapper now accept LOAD (10, capability
`4000h`) and UNLOAD (11, `8000h`). Each issues one synchronous sequential
`1Bh` command with zero count/flags/block size. The complete current provider
mask, including removal controls below, is `1FFFFFFFh`; individual bits still describe provider support, not
confirmed drive capabilities. No implicit readiness check, rewind, marks,
retension or retry occurs. Successful media completion (`0003h`) claims
neither BOT nor readiness. A physically missing cartridge still needs manual
insertion. Changer control remains separate.

Active writers and legacy/buffered/rewinding sessions remain BUSY. Explicit
LOAD/UNLOAD are allowed for recovery after a failed/abandoned native session,
but preserve FAILED (`40h`) and DIRTY|FAILED (`60h`). Success of a media action
cannot finalize abandoned output. The CLI prints the retained-state warning
even when the media action succeeds (exit 0); native status remains exit 2.
Only explicit successful native REWIND clears the native uncertainty. A failed
media command reports `0005h`, retains FAILED, and is never replayed.

`dos-native-media-smoke` qualifies provider, SDK and localized CLI in disposable
DOS images with a memory-only manager; see [testing](testing.md). Physical-drive
and Windows behavior remain unqualified. OpenASPI's `docs/native-tape-media.md`
defines the full wire and recovery contract.

## Native cartridge removal control

```text
OPENTAPE --native-lock --device NRMT0
OPENTAPE --native-unlock --device NRMT0
```

LOCK (12, provider capability `00010000h`) and UNLOCK (13, `00020000h`)
use the existing `ot_native_execute` and DOS wrapper. The complete provider
mask is now `1FFFFFFFh`; retain the full DWORD rather than truncating to a WORD.
They each submit one synchronous six-byte `1Eh` PREVENT/ALLOW MEDIUM REMOVAL
command, without data, retries or other media operations.

These commands control cartridge removal, independently of write protection
and native writer claims. Active writers/multiple handles/legacy buffered or
written state remain BUSY. Abandoned FAILED/DIRTY|FAILED native state survives
successful and failed removal commands. Success has completion `0003h`, failure
`0001h`: failed LOCK/UNLOCK introduces neither POSITION_UNKNOWN nor native
FAILED because it does not move tape. No cached lock state is invented after
an ambiguous command failure. A successful command preserves queued legacy
EOF. Rewind does not implicitly unlock the drive. Failed explicit UNLOAD still
uses the existing media-command uncertainty rules.

A build against older provider headers reports that removal controls require
a newer SDK, without opening a device or interpreting the verb as READY. A
matching-header build checks each operation's individual capability bit, so
older running providers fail as unsupported before a control submission.
`dos-native-lock-smoke` covers strict packets, high-WORD bits, ownership,
clean/dirty abandoned writers, prevented UNLOAD, legacy EOF, error classes and
localized CLI behavior. OpenASPI's `docs/native-tape-removal.md` has the wire
contract. Physical-drive qualification and automatic locking around archive
jobs remain pending.

## Non-moving logical position observation

```text
OPENTAPE --native-position --device NRMT0
```

The optional `ot_native_position` / `ot_dos_native_position` API uses a separate
64-byte position packet and DOS function 4. Capability `00040000h` brings the
complete current provider mask to `1FFFFFFFh`. Each query submits one ten-byte
READ POSITION (`34h`) in short logical form; no movement, flush, mark, retry,
implicit LOCATE or six-byte vendor fallback occurs.

Verified replies expose partition and both full unsigned 32-bit logical object
locations. These are device coordinates, not byte offsets or archive file IDs.
Raw BOP/EOP describe the reported partition and do not claim native rewind
completion. BPU/LOLU and PERR suppress coordinate validity; zero without validity
is unknown. Buffer counts are not exposed. The short format has a 32-bit limit;
larger formats remain pending; explicit LOCATE(10) is described below.

A negotiated residual must prove the complete 20-byte response. Short/malformed
or missing residual reports return a checked command error. A legacy manager
without residual support returns PARTIAL/INCOMPLETE with UNVERIFIED_LENGTH and
no coordinates. Successful hardware observation may also report unknown
position; the SDK returns PARTIAL and the CLI exits 2 when no verified location
is available. A valid address prints normally with CLI exit 0.

Active writers, legacy buffered/written/error state, pending rewind and multiple
handles remain BUSY. Retained native FAILED/DIRTY|FAILED is inspectable but never
cleared by a reported address. Legacy diagnostics and queued EOF are preserved.
A failed observation does not introduce POSITION_UNKNOWN or native FAILED.
Current/deferred sense is decoded after the actual ten-byte CDB rather than at
the six-byte offset. Builds against older SDK headers reject the new verb
without opening a device. `dos-native-position-smoke` covers the provider, SDK
and localized CLI; see [testing](testing.md). OpenASPI's
`docs/native-tape-position.md` defines the wire contract and references.

## Explicit logical LOCATE(10)

```text
OPENTAPE --native-locate 12345 --device NRMT0
OPENTAPE --native-locate 4294967295 --partition 2 --device NRMT0
OPENTAPE --native-position --device NRMT0
```

`ot_native_locate` and `ot_dos_native_locate` use a separate 64-byte target
packet (DOS function 5). Capability `00080000h` brings the current mask to
`1FFFFFFFh`. Addresses are unsigned 32-bit logical objects, not byte offsets
or archive file IDs. Flags 0 select the current partition and require request
partition zero; `OT_TAPE_LOCATE_PARTITION` explicitly selects partition 0..255.
The caller establishes that an explicit target partition exists. No automatic
partition probe, creation or selection is added. A drive may flush its own
pending buffer during LOCATE; no separate flush command is submitted.

Exactly one synchronous ten-byte LOCATE (`2Bh`) is submitted without data,
IMMED, BT, readiness/position probes, flush, mark, retry or rewind/SPACE fallback.
The SDK checks character-device identity, the individual capability, clean
session state, guarded packet bounds and the exact echoed target, then closes
once while preserving the first error. Old SDK-header builds reject the new
verb without a device open. Decimal operands reject signs, overflow, missing
and duplicate options before I/O. `--partition` belongs only to LOCATE.

Active writers, legacy buffered/written/error state, cached data, multiple
handles, pending rewind and retained native DIRTY/FAILED remain BUSY. Success
has completion `0003h`, clean state and no BOT claim, even for target block 0.
Successful movement clears queued legacy EOF. Failure has `0005h`, diagnostics
and native FAILED; even a valid later READ POSITION does not authorize another
movement or native data I/O. Explicit successful native REWIND is still the
recovery operation. It does not force partition 0: the historical BOT flag
means the selected partition's beginning. The CLI now makes this distinction.

`dos-native-locate-smoke` checks cursor/partition readback independently through
READ POSITION, unsigned limits, invalid packets, active/abandoned writers,
legacy EOF, ambiguous failed movement with altered cursor, first-error cleanup,
fixed/descriptor/deferred sense, exact commands and both languages. See
[testing](testing.md) and OpenASPI's `docs/native-tape-locate.md` for qualification
and the complete contract. Wide bindings are described below; verified emulation, archive resume and
physical-drive qualification remain pending.

## 64-bit logical positioning

```text
OPENTAPE --native-position64 --device NRMT0
OPENTAPE --native-locate64 4294967296 --device NRMT0
OPENTAPE --native-locate64 18446744073709551615 --partition 2 --device NRMT0
```

`ot_native_position64` / `ot_dos_native_position64` read the 32-byte long
READ POSITION response (service action `06h`). `ot_native_locate64` /
`ot_dos_native_locate64` submit synchronous LOCATE(16), opcode `92h`.
Each has an explicit format-1 64-byte packet on the established function 4/5;
format 0 and its old packet layouts are unchanged. Capabilities `00100000h`
and `00200000h` advertise these wide operations. The complete current provider
mask, including PARTITIONS, is `1FFFFFFFh`. Wide support is
checked independently; no automatic short-form fallback or truncation occurs.

Coordinates use explicit low/high DWORDs throughout the portable SDK and DOS
binding. Shared decimal conversion supports `0..18446744073709551615` using
16-bit limbs and 32-bit intermediates, without floating point or `long long`.
Overflow and malformed operands are rejected before device I/O. Explicit
partition selection still accepts 0..255; current-partition mode remains
separate. A long observation can report a wider partition field without making
it a representable LOCATE partition selector.

Long-form LONU and MPU independently qualify the block/partition and logical
filemark-number groups. Unknown fields are zeroed without claiming a valid
zero. Verified block position may succeed with unknown filemark number; no
verified block returns SDK PARTIAL/CLI 2. A legacy manager without residual
proof exposes no coordinates. Existing uncertainty, native writer ownership,
legacy EOF and saved diagnostics follow the same observation/movement rules
as the short bindings. A failed LOCATE(16) latches native FAILED and is never
replayed; observation cannot clear it. Autosense follows the actual six-, ten-
or sixteen-byte CDB rather than a fixed offset.

Host/ABI/SDK and `dos-native-wide-smoke` tests exercise full-width readback,
independent validity, decimal limits, unknown short-form overflow, large
partitions, legacy/malformed lengths, moved cursor after failure, exclusive
ownership and explicit recovery in both languages. See [testing](testing.md)
and OpenASPI's `docs/native-tape-wide.md`. Hardware and Windows qualification,
64-bit archive bookmarks and verified emulation remain pending.

The [DOS native control screen](native-controls-ui.md) now consumes these
operations through the same SDK: T opens the subview, P reads position and G
edits a full-width target with explicit confirmation. Load/unload, removal
locks and rewind share the established ownership and failure rules.

## Reported partition layout

`OPENTAPE --native-partitions [--device NRMT0]` uses
`ot_native_partitions` / `ot_dos_native_partitions` and function 6. It prints
raw maximum/defined additional counts, flags, format recognition, packed
units and size descriptor count only after validating the complete response.
The result may describe a pending configuration or previous medium: it is
not a list of proven LOCATE targets, capacity or LTFS support. Native failure
state and legacy EOF/diagnostics are preserved. Unproven transfer lengths
return PARTIAL with no layout. No readiness probe, retry, mode change or
formatting is implicit. Older headers reject the command without device I/O.
See OpenASPI `docs/native-tape-partitions.md` for the exact wire contract.

The DOS tape-controls **S** view now exposes signed record/filemark SPACE and
EOD through the same adapter. `ot_native_space_count_parse` supplies the shared
CLI/UI decimal parser; malformed input preserves its output. Unit choice,
editing and canceled confirmation perform no I/O. See
[relative positioning in the DOS controls](native-controls-ui.md#relative-positioning).

## Extended mark spacing

The native SDK and CLI expose `--native-space-seq-filemarks N`,
`--native-space-setmarks N` and `--native-space-seq-setmarks N`. Each uses
signed decimal -8388608..8388607 and optional `--device NRMT0`. CONTROL enums
14/15/16 map to SPACE codes 2/4/5, respectively. The individual provider bits
are `00800000h`, `01000000h` and `02000000h`; the complete current provider
mask is `1FFFFFFFh`. The 64-byte packet, version and count offset are unchanged.

Consecutive spacing seeks N adjacent matching marks, rather than counting N
separate runs. Setmarks remain distinct from filemarks. All operations use the
current partition and issue one synchronous CDB. They neither write marks nor
automatically change setmark-reporting mode. Device/mode support is qualified
separately; unsupported commands fail with retained uncertainty and no replay.
This does not change archive framing. Classified setmark reads now have
the separate boundary contract below.

`OT_NATIVE_TAPE_EXTENDED_SPACE` identifies matching provider headers.
`ot_native_space_operation` classifies supported SPACE enums without including
media/removal enums 10..13. Older SDK builds reject CLI verbs and chooser
entries 4/5/6 without opening a device or substituting READY.

## Classified setmark reads

Matching provider headers expose `OT_TAPE_STREAM_SETMARK=40h` and provider
capability `OT_TAPE_OP_READ_SETMARK=04000000h`. Current target mark sense with
ASC/ASCQ `00/03` is a setmark rather than an ordinary filemark. A current
SDK reader returns its validated STREAM event and actual bytes, including
data-plus-mark replies. Hard, deferred, host or malformed responses remain
errors; absent count proof does not become an empty-record success.

The shared validator rejects older providers' FILEMARK+`00/03` packets and
blocks that reader session. Older SDKs reject the new unknown flag without
reading again. OTB1 archive readers reject a classified setmark as a boundary
error in both variable and fixed modes, without accepting data, queuing EOF
or performing another read. Verification cannot complete on this mark.
The existing archive format and legacy compatibility API are otherwise retained;
explicit setmark writing now uses the owned writer operation below; device
mode changes remain separate work. See OpenASPI
`docs/native-tape-setmarks.md` for the precise protocol and provenance rules.

## Explicit setmark writing

`OT_NATIVE_TAPE_WRITE_SETMARK` enables `ot_native_writer_setmark` and
`ot_dos_native_writer_setmark` with matching provider headers. CONTROL operation
17 writes one setmark with WSMK=1 and IMMED=0. It requires an active writer and
zero packet count/flags/block-size fields even for a fixed-block claim. It is
not accepted by the one-shot `ot_native_execute` or SPACE entry points.

The independent provider bit is `OT_TAPE_OP_SETMARK=08000000h`. Writer open
captures the snapshot operation mask; a missing bit rejects this optional
request without a command or failure latch, leaving ordinary writing available.
A submitted setmark marks the session dirty before I/O, retains ownership on
success, and retains dirty/failed ownership on error. Further operations do not
retry it. Abort/unfinished close release ownership without marks/flushes and
preserve failure through cleanup; explicit rewind supplies later recovery.

Ordinary OTB1 finalization continues to use its required filemark and does not
insert setmarks automatically. An older-header build keeps ordinary writer
behavior. The provider's `docs/native-tape-setmark-write.md` documents wire
fields, completion states and memory-only qualification.

## Explicit TapeAlert retrieval

`ot_native_alerts` / `ot_dos_native_alerts` require the explicit
`OT_TAPE_ALERTS_ACKNOWLEDGE` flag. Reading page 2Eh can acknowledge device
warning flags; the SDK never polls/retries it as part of ordinary status.
The provider capability is `OT_TAPE_OP_ALERTS=10000000h`; DOS function 7 uses
a separate 64-byte packet. Reported and active masks contain flags 1..64
through separate low/high DWORDs. Active is a subset of reported; an absent
reported bit is unknown, not clear. State helper results are -1/0/1.

`OPENTAPE --native-alerts --allow-clear [--device NRMT0]` prints and flushes
the notice before submission. It rejects missing/duplicate acknowledgment and
refresh/extra arguments without opening. Exit 0 means complete retrieval,
including a report with active flags; it is not a drive-health verdict.
No warning severity or recovery policy is invented from numeric codes.

The driver stages masks until the complete page passes bounds/parameter checks.
Unverified legacy transfer lengths return PARTIAL with no flag states. Errors
preserve legacy diagnostics, queued EOF and native uncertainty. A first read
may already have cleared warnings even if its reply cannot be used, so no
second request is made automatically. Old headers reject the CLI/UI action
without substituting READY. See OpenASPI `docs/native-tape-alerts.md` for the
wire contract and primary device references.

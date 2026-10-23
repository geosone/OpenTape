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
completed read with FILEMARK/EOM/ILI/EOD: inspect `reader.last` for both actual
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
and EOD using CONTROL operations 7..9. A conditional count DWORD occupies
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
mask is `0000FFFFh`; individual bits still describe provider support, not
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

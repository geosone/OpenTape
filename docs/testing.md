# Testing OpenTape

`make MAXLANG=US check` and `make MAXLANG=GR check` build the DOS and Win16
frontends and run host tests for backend selection, the native-format codec
and read-only tape probing/reading. Probe fixtures cover modern/legacy status,
normalization, invalid/rewinding names, missing devices, ordinary files,
guard corruption, detected incompatible legacy layouts and cleanup/error
precedence. No host unit test accesses a tape.
Reader tests cover raw-mode setup, short data, filemarks and continuation,
fixed-block alignment, invalid counts, failed-state/no-retry behavior,
diagnostic failure precedence and cleanup. They also pass with AddressSanitizer
and UndefinedBehaviorSanitizer on the host.
Native-scanner tests cover valid labels and unknown records, every truncated
prefix of a label block, block/fragment CRC errors, record-count/length and
sequence mismatches, duplicate/bad labels, invalid fragment offsets and counter
overflow. A host-only test validates a full 65,535-byte block with outer guards.
Native scanner tests also pass with both host sanitizers.
Typed-payload tests cover little-endian reference bytes, session/file start/end
roundtrips, DOS attributes and calendar dates, path length boundaries, truncated
metadata, reserved fields and device/traversal names. The scanner must reject
malicious typed payloads even when all block and fragment CRCs are valid, without
committing partial state. Both host sanitizers also cover these payload tests.
Archive-state tests additionally cover fragmented whole-file CRCs against the
standard `123456789` reference, empty files, directories, multiple sessions,
wrong IDs/times, premature endings, gaps/duplicate fragments, bad totals,
unsupported records and whole-block rollback. Counter overflow/carry cases
inject boundary state; they do not claim multi-gigabyte tape qualification.
The same state tests pass with both host sanitizers.
Writer tests use a memory sink and then feed the captured physical blocks into
a separate verifier instance. They cover known whole-file CRCs, empty files,
directories, multiple sessions, scratch-buffer aliasing, short/error/invalid-count
writes, delayed finalization errors and rejected reentrancy. A host-only guarded
test emits a 65,535-byte block; the host writer tests also pass with both sanitizers.

## DOS status integration

`make dos-status-smoke` boots the actual OpenTape DOS executable using a copy
of an externally prepared OpenASPI dc390/mhVTL boot image. Prepare that image
with OpenASPI's `dos-mhvtl-smoke` setup: it must load the matching external DOS
ASPI manager and `ASPITAP.SYS`, using `DELAY.SYS` for delayed target hotplug.
See [OpenASPI's setup guide](../deps/OpenASPI/docs/qemu-mhvtl.md).
No vendor manager or boot disk is redistributed. The template is copied,
not edited. The changer is only queried for identity; it is not passed to DOS.

For the currently verified local virtual setup:

```sh
DOS_BOOT_IMAGE=/home/mario/mars/OpenAspi/build/dos-mhvtl/boot.img \
MHVTL_TAPE=/dev/sg6 \
MHVTL_VERIFY_CHANGER=/dev/sg13 \
MHVTL_VERIFY_DRIVE=0 \
MHVTL_SCRATCH_BARCODE=OA2101L8 \
make MAXLANG=US dos-status-smoke
```

Repeat with `MAXLANG=GR`. These node and barcode values are local examples,
not portable defaults. The test deliberately reuses OpenASPI's strict mhVTL
identity gate even though the application only requests status. Physical
devices are refused. It performs no media changes, writes or motion commands.
Use a template whose no-rewind aliases follow the expected first-drive setup;
units 2 and 3 must not already be installed.

The batch checks help, default/explicit `NRMT0` status, rejection of `RMT0`,
an unavailable unit and a regular file named `NRMT3`. The file must remain
byte-identical. A checked DOS batch marker, expected ASPI address and QEMU
debug-exit status are required. Logs/images live under
`build/US/dos-status/` or `build/GR/dos-status/`. Do not run two tests for the
same language concurrently. No local filemark patch is needed for status.

The English and German DOS status tests passed on 2026-10-22 with the installed
unmodified QEMU 10.2.3, Tekram ASPI 1.60 and mhVTL's IBM ULT3580-TD8. Legacy
fallback is host-fixture tested, not yet qualified with the historical driver.
The Windows frontend is build-tested only; it does not use this DOS backend.

## DOS data-read integration

`make dos-read-smoke` extends the same integration harness with a bounded data
read. It requires the known final buffered-roundtrip layout written by the
current OpenASPI `dos-mhvtl-roundtrip` test on the approved disposable virtual
cartridge. That layout is three 1024-byte physical records containing 2,500
patterned bytes and 572 zero padding bytes, followed by filemarks. The read
test does not create or overwrite this fixture.

Use the same environment as above, but additionally select
`QEMU_BIN=/path/to/patched/qemu-system-i386` and call `make MAXLANG=US dos-read-smoke`
(repeat with `MAXLANG=GR`). The tested QEMU 10.2.3 filemark workaround is
described in OpenASPI's setup guide. Installed unmodified QEMU is suitable for
status only, not this filemark test.

The harness revalidates the virtual cartridge and explicitly rewinds it with
host `sg_raw` before booting; this moves the tape but writes no media data.
Only the read-test mode enables this motion. The application itself has no
rewind in `--read-test`. Wrong/buffered names and regular files must be rejected
before reading. Acceptance requires three 1024-byte records with CRC32 values
`227BE3D1`, `76A5CF1D`, `4D13C3E7`, then a filemark and a checked DOS exit.
These expected values come from the known transport fixture, not an OpenTape
backup. Do not interpret them as native-format compatibility or restore tests.

English and German read tests passed on 2026-10-22 through Tekram 1.60 and
mhVTL with the local QEMU patch. Logs are under `build/US/dos-read/` and
`build/GR/dos-read/`. The final tape position is after the first filemark;
no automatic cleanup rewind is performed. No physical tapes were tested.

## Native framing under DOS

All integration modes additionally run `NATTEST.EXE`, a DOS build of the same
native-scanner positive/negative fixtures (excluding the large host-only boundary
case). This test executable runs entirely in memory and is not distributed
as part of the user application. Its success marker and DOS exit are required.
They also run `PAYTEST.EXE`, a DOS build of the typed-payload tests, with a
separate required success marker/exit. These in-memory tests do not write tapes
or create restored files. English and German integration runs passed with both
test programs on 2026-10-22.
`VERTEST.EXE` now runs the archive-state fixtures under DOS as well, including
positive closed sessions and deliberately incomplete/corrupt sequences. Its
success marker and DOS exit are separately required. English/German runs passed.
`WRITEST.EXE` additionally runs the memory-sink writer/readback fixtures under
DOS, excluding the host-only maximum-block allocation. Its own marker and DOS
exit are required. No writer test is connected to the tape device: the integration
harness still only performs its existing explicit rewind and read/status probes.
Both English/German DOS runs passed on 2026-10-22 with the memory writer enabled.

`make dos-native-smoke` selects a separate integration mode using the same
environment and known non-native scratch fixture as the data-read test. After
identity verification it explicitly rewinds that virtual cartridge on the host.
The actual `OPENTAPE --scan` and `--verify` commands run in succession and must
each reject a non-native record with bad magic and exit `2`; raw driver-test
bytes must never be mistaken for an OpenTape volume. English/German tests
passed on 2026-10-22; logs are in
`build/US/dos-native/` and `build/GR/dos-native/`.

These tests establish positive framing parsing in DOS memory and negative
format recognition through DOS ASPI/mhVTL. No valid native tape has been written
or successfully scanned end-to-end yet. This is not backup/restore qualification.
The native rejection test leaves the tape after the second physical record;
the raw-read regression leaves it after the first filemark. No media is written.

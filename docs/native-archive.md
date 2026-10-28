# Archive commands through the native tape API

The DOS frontend can now use the native OpenTape API for backup, reading,
verification, listing, restore, restore preview and comparison. Select it by
placing `--native` before the existing archive command. Native selection is
explicit; an unavailable or failing native service never causes a legacy retry.
The existing ASPITAPE backend remains available through the ordinary commands.

```text
OPENTAPE --native --backup-file INPUT.BIN --as DATA/INPUT.BIN --device NRMT0 --overwrite --volume TEST
OPENTAPE --native --backup-tree SRC --as DATA --device NRMT0 --overwrite --volume TEST
OPENTAPE --native-rewind --device NRMT0
OPENTAPE --native --verify --all --device NRMT0
OPENTAPE --native --list --device NRMT0 --selection-out FILES.LST
OPENTAPE --native --restore --device NRMT0 --to OUT --dry-run
OPENTAPE --native --restore --device NRMT0 --to OUT
OPENTAPE --native --restore --device NRMT0 --to OUT --replace DATA/INPUT.BIN
OPENTAPE --native --compare --device NRMT0 --to OUT
```

Each reading command starts at the current position. Rewind explicitly before
each new pass. `--native --scan` and `--native --read-test` retain their existing
bounded-read behavior. Backend selection does not change exact replacement
selection, preview, staged restore/readback checks or source-file validation.
`OPENTAPE --native --ui` selects the existing QUILIB interface with native
status, alias discovery, confirmed rewind, archive verification and catalog
export. The selected API is shown on the device and catalog screens. Snapshot
queries never issue readiness/movement commands. Failed rewind reports unknown
position without replay; unfinished writer state is shown until explicit
recovery. An unrelated command after `--native` is rejected before tape access. Both English and German DOS builds provide usage
and unfinished-session messages.

## Fixed-block archives

Select a drive already configured for fixed blocks, then put the explicit size
between `--native` and the archive command:

```dos
OPENTAPE --native --fixed-block 512 --backup-file INPUT.BIN --as DATA/INPUT.BIN --device NRMT0 --overwrite --volume TEST
OPENTAPE --native-rewind --device NRMT0
OPENTAPE --native --fixed-block 512 --verify --all --device NRMT0
OPENTAPE --native-rewind --device NRMT0
OPENTAPE --native --fixed-block 512 --restore --device NRMT0 --to OUT
OPENTAPE --native --fixed-block 512 --ui
```

The DOS frontend supports 1..4096 bytes per physical block with a resident
4096-byte workspace. The common archive adapter supports the native SDK's
1..65535 range when the caller supplies enough separate workspace. The size
is explicit for each invocation, including reads; no mode guessing, MODE SELECT
or fallback is performed. UI device/catalog screens show the selected block size.

Each logical OTB1 block occupies a whole number of physical blocks. The final
physical block is zero-padded; the next OTB1 block starts at the next physical
boundary. Logical length, CRCs and record bytes are unchanged. Read assembly
validates the header CRC before trusting its length, bounds it by the caller's
capacity, validates the payload CRC and rejects nonzero padding. Headers may
span physical blocks, and logical blocks may span multiple I/O requests.

A filemark inside an incomplete logical block fails. Data plus a filemark at a
complete boundary delivers data first and that mark on the next logical read.
Malformed framing is reported as an archive error; host/target/unknown-count
failures remain transport errors. Both stop further reads and require cleanup.
No partial write, padding failure or uncertain operation is replayed.

Library callers use `ot_archive_native_reader_fixed` /
`ot_archive_native_output_fixed` with a live native session and disjoint
caller-owned workspace. Ordinary attach calls still reject a fixed session
without workspace. The archive-facing status describes reconstructed logical
blocks, while physical mode/count details remain in the native session.

## Archive stream rules

The native bridge connects existing archive readers/writer sinks to the shared
native sessions. It keeps one handle open across records. A native read with
both bytes and FILEMARK supplies the data first and delivers that same mark on
the next archive-level read, without another SCSI command. An ordinary empty
record is never treated as a filemark. EOM/EOD cannot establish archive
completion: verification/restore requires the explicit filemark and complete
framing, file checksums and session metadata.

Partial data accompanying a native error is retained for diagnosis, not passed
to the archive decoder as successful input. Truncated records and failed reads
stop the stream without replay. Visitor/format errors also close without
advancing the medium or automatically seeking elsewhere.

After a valid SESSION_END record, the native writer sink performs:

1. Synchronous FLUSH, writing zero marks.
2. One explicit FILEMARK as the archive delimiter.
3. END_WRITE, flushing with zero marks and releasing ownership.
4. One handle close.

Only success through the final close produces a completed backup result.
Failures in record output, source processing or finalization trigger cleanup
that aborts ownership; cleanup never flushes, adds marks, backspaces or rewinds.
The first failure and its native record/control packet survive cleanup. An
unfinished native writer also makes `--native-status` exit with code 2 and
report the retained failed state. Recovery still requires an explicit release
and successful native rewind; status/readiness alone does not repair position.

## Build, tests and remaining scope

Use matching OpenASPI headers and provider, as described in
[native-tape-api.md](native-tape-api.md). The current pinned older submodule
still builds the legacy frontend; the live sibling checkout enables native
commands:

```sh
make OPENASPI=../OpenAspi native-archive-test
bash tests/dos_native_archive.sh
MAXLANG=GR bash tests/dos_native_archive.sh fixed fixed-ui
bash tests/dos_native_archive.sh fixed-legacy fixed-padding fixed-short fixed-end
```

The host integration test exercises the real archive writer and verifier over
native sessions, including combined data/filemarks, empty records, partial
errors and each failed finalization stage. Fixed-mode cases cover header
reassembly, padding, truncation, invalid lengths and the 65535-byte logical /
65536-byte physical boundary; host memory-sanitizer checks cover those buffers. DOS tests run the actual CLI against
a resident memory-only ASPI medium that retains records across open/close and
rewind. They compare restored bytes with a 35-KB binary source, check directory
backups and selected replacement, and test old ASPI managers, combined marks,
EOM/EOD, corrupt records, hard reads, short writes and failed flush/mark/end.
Exact command counts detect extra reads, retries and implicit close-time marks.
Ordinary-file rejection preserves both source and rejected device files.
The tests create disposable QEMU floppy images and never access physical tape.

Fixed-block archive framing is now implemented through explicit attachments
and CLI selection; it does not activate by guessing a block size. Windows
bindings and physical fixed-drive qualification remain open.
Device-specific behavior will be implemented later as a separate, qualified
quirks layer below these archive rules. This milestone adds no vendor-name
heuristics, guessed workarounds or device-specific command fallbacks.

The `ui`, `ui-failed`, `ui-unfinished` and `ui-legacy` fixtures in the same DOS
harness use real QEMU keyboard input and inspect VGA text memory, including
API labels, snapshot fields, alias errors, confirmation/cancel, rewind result,
verification, catalog export and retained writer failure/recovery. Native UI
status/enumeration emits zero SCSI commands; exact total command counts guard
against extra reads or rewind retries. English and German native UI variants
are exercised. Shared VGA/keyboard helpers also serve the older UI harness.

A common implementation bug is fixed in the shared path. Device/firmware
defects get narrowly qualified quirks. Useful uncommon functions belong to the
native API and can receive a direct implementation or a verified portable
emulation, with provenance/support reported. Those three categories remain
separate; no optional-function emulation or runtime quirk is enabled by the UI
work.

A classified native SETMARK is not an OTB1 delimiter. Variable and fixed
readers fail that archive read without accepting bytes or queuing FILEMARK,
including a mark delivered alongside a complete final data block. Raw native
SDK clients can still observe the typed event and count. Host and DOS CLI
tests verify that a valid session followed by a setmark cannot pass as a
filemark-terminated archive.

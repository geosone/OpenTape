# DOS tape output transport

`core/tape_output.c` adapts the native writer sink to DOS character-device
writes. `ot_dos_output_ops()` reuses the existing critical-error handling,
status and raw-mode functions. The experimental
[single-file CLI](single-file-backup.md) now uses this sink.

The caller must zero-initialize the transport and explicitly authorize writing
before opening. Without authorization there are no device calls at all. Only
raw no-rewind names are accepted. Read-only preflight rejects ordinary files,
buffered/fixed-block devices, rewind-on-close mode, active write/control state
and saved errors before writable open. Writable open repeats the device/mode
checks and selects DOS binary mode. No file is created or truncated.

This initial output backend requires the extended 16-byte status layout.
Legacy read support is unchanged; older drivers lacking saved finalization-error
reporting are deliberately not accepted for this write path.

Each block uses one write with an exact count and a subsequent status query.
Errors, short writes and impossible counts poison the transport; no retries or
further writes are allowed. Preserve the transport and writer error diagnostics
and close for cleanup even after failure. The first write failure remains the
reported error. Media may already have changed, so cleanup is not rollback.

Successful session finalization closes the writable handle once. ASPITAP owns
the two closing filemarks and no-rewind backspacing; OpenTape issues no extra
MTWEOF. A separate read-only status probe then checks saved write/finalization
errors, even when DOS close reported success. This transport is single-use and
supports one session; a second session needs a newly authorized transport.

There is no implicit rewind, positioning recovery, media identity detection,
hardware write-protect query or physical durability guarantee. Authorization
alone does not prove that the correct tape is mounted. The future command must
establish identity/confirmation and positioning policy before opening. A
write-protected medium is currently diagnosed by the failed write and driver
status, not by a preflight WP bit (the character-device status has none).

Host and DOS `OUTTEST.EXE` tests use injected operations, not media. They cover
authorization refusal, forbidden aliases, file rejection, incompatible modes,
legacy-status refusal, exact/short/impossible/error writes, delayed errors and
single-shot close.

`TAPETEST.EXE` is a separate, non-distributed destructive diagnostic. With
`--write --overwrite --device NRMT0` it writes a deterministic native session
at the current position; without every required argument it exits before device
access. It does not itself establish cartridge identity. The host harness
requires `MHVTL_ALLOW_OVERWRITE=OA2101L8`, validates that exact virtual cartridge
and confirms a clear MODE SENSE write-protect bit before launching DOS.

The integration fixture contains eleven variable records: a volume label,
session, directory, 2,500-byte binary file in three chunks, empty file and
session completion. A separate boot after an explicitly gated host rewind uses
`--verify --device NRMT0` to compare every record byte-for-byte with regenerated
expected output, independently verify structure/CRCs, and require the next
record to be a filemark. A third boot runs the application's normal `--verify`.
Only the approved virtual scratch cartridge is overwritten; there are no
changer commands, protection changes or accesses to physical tapes.

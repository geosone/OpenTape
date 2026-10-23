# Read-only ASPITAP alias overview

The same enumerator is available interactively with D in the
[`OPENTAPE --ui` device/status screen](dos-interface.md).

```text
OPENTAPE --devices
OPENTAPE --devices --letter S --dollar
```

The default queries exactly `NRMT0` through `NRMT3`. `--letter S` selects the
configured `-tS` namespace instead of M; ASCII letters are case-insensitive.
`--dollar` appends `$`, so the second example queries `NRST0$` through `NRST3$`.
Options may appear once each, in either order. Other namespaces, naming variants
and unit numbers are not tried automatically.

This enumerates supported **installed aliases**, not controllers or all physical
devices. It never loads/configures a driver, selects a unit, reads tape data,
polls readiness, rewinds, spaces, writes or operates a changer. It reuses the
status-only probe: read-only open, DOS character-device check, guarded status,
and one close attempt before the visitor. Run no competing tape application.

## Results and exit codes

- `FOUND`: a supported guarded snapshot and successful close. The row includes
  ASPI address, state, fixed-block size and ABI. Legacy status shows `driver=--`
  because it has no extended error byte. FOUND is **not** media, readiness,
  write-protection, ownership or transport-suitability validation. Saved errors
  may still be present in a valid snapshot.
- `MISSING`: open returned DOS file-not-found (`2`), not an access/path/driver error.
- `REJECTED`: an ordinary same-named file, rejected before tape IOCTL. Its content
  is not read or changed.
- `ERROR`: another open/type/status/ABI/close failure, with probe and DOS errors.

Counts are aliases, not distinct drives. Equal ASPI addresses are deliberately
not deduplicated, and a snapshot never automatically selects a device. Each
later operation still performs its own status checks. The existing guarded
16-byte status with supported 14-byte fallback is reused unchanged.

Rows are provisional. Output is checked/flushed per row, and observed output
failure stops before the next alias. Independent probe failures are reported
while the remaining aliases are still examined. Summary counts retain partial
progress. Completion requires all four aliases and checked output; require the
exit code as well as a footer, since a late output failure may deliver partial text.

Exit `0` means a complete error-free scan with at least one FOUND alias; `1` is
invalid syntax; `2` is rejected/error entries or output/internal failure; `3`
means all four aliases are absent. A found alias plus a rejected lookalike file
returns `2`, not success. No automatic retry or fallback namespace scan occurs.

## Shared API and tests

`ot_tape_enumerate` accepts only `ot_probe_ops`, without data or control callbacks.
Visitors receive transient entries; copy them if retained for future UI use.
A nonzero visitor result stops after the current close attempt and before the
next open. Host probe tests and DOS `PRBTEST.EXE` share 14 fixtures covering
normal/all-missing/mixed scans, letters/dollar names, ordinary-file rejection,
open/info/status/ABI/close errors, legacy fallback, visitor failure, duplicate
ASPI addresses and invalid inputs refused before any open.

`make dos-devices-smoke` checks the approved OA2101L8 environment: a regular
NRMT3 lookalike gives partial/rejected status, absent namespaces return `3`, and
a clean namespace returns `0`. The batch temporarily renames only that disposable
test file out of the namespace, then restores its name and checks its original
bytes. These fixture renames are not application discovery operations.
After all queries, the archive must still verify from its label through all
95 records without an application rewind, demonstrating unchanged tape position.
Only the harness explicitly positions the guarded virtual fixture before boot.
No tape data is written; physical/controller discovery remains unqualified.

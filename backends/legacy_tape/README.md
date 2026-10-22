# Legacy tape backend

This is the mandatory interoperability backend for historical `ASPITAPE.SYS`
and OpenASPI's compatible `ASPITAP.SYS`.

## Read-only status

`OPENTAPE --status` queries `NRMT0`; `--status --device NAME` selects a
different no-rewind alias. Lowercase names, units 0 through 3, `-tX` names
such as `NRST0`, and optional `$` suffixes are accepted. `NMT0` buffered
aliases are also permitted. Rewinding aliases, paths, drive letters and
ordinary DOS names are rejected before opening. DOS character-device identity
is checked before querying status, so a regular file named `NRMT0` cannot
be mistaken for a tape.

The probe opens read-only and issues only device-information and category
`07h` status IOCTLs. It does not set raw mode, transfer data, poll readiness,
rewind, space, write filemarks or format media. It queries function `05h`
first for OpenASPI's explicit 16-byte status, falling back to function `02h`
for unsupported-function errors. The fallback uses the documented 14-byte
manual ABI; the historical alternative 16-byte ABI at `02h` is not supported.
Both destinations have guard bytes; detected writes beyond the selected
ABI are rejected rather than parsed. This is not general ABI autodetection.

Legacy state bits are normalized to the 16-byte state layout. The `extended`
indicator distinguishes a valid driver-error field from legacy status, which
has none (the displayed driver error is then unavailable). Saved errors
remain visible; a successful status query does not imply ready, healthy media.
Only an explicit successful function `05h` identifies the extended status
ABI; backend priority does not silently replace the caller's driver.

Every successfully opened handle is closed, including on failures. The first
error is preserved if close also fails. A temporary DOS critical-error handler
returns Fail, never Retry, and restores the prior INT 24h vector afterward.
CLI exit codes: `0` status retrieved, `1` invalid arguments, `2` probe failed.

`core/tape_probe.c` owns portable probe/fallback policy. `legacy_dos.c` supplies
the DOS calls; host tests inject an operation table. There is intentionally no
data-write or tape-motion callback in this read-only interface. Data streaming,
raw-mode setup and explicit media controls will be separate operations.

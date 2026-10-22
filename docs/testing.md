# Testing OpenTape

`make MAXLANG=US check` and `make MAXLANG=GR check` build the DOS and Win16
frontends and run host tests for backend selection, the native-format codec
and read-only tape probing. Probe fixtures cover modern/legacy status,
normalization, invalid/rewinding names, missing devices, ordinary files,
guard corruption, detected incompatible legacy layouts and cleanup/error
precedence. No host unit test accesses a tape.

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

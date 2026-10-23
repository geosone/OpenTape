# Explicit DOS rewind

```text
OPENTAPE --rewind --device NRMT0
```

The default device is `NRMT0`. This command explicitly changes tape position
to beginning of tape using ASPITAP's synchronous `MTREW` operation. It does not
read or write archive records, write filemarks, erase, unload, change partitions
or operate a changer. It is not a media-identity check or a lock against another
application. Close other tape applications first, check the selected unit, and
do not rewind a drive with a concurrent reader or writer.

The read-only [`--devices`](device-enumeration.md) overview helps inspect
installed aliases. FOUND is not media/readiness validation and never authorizes
automatic rewind or writing.

The [DOS interface](dos-interface.md) also exposes this same synchronous
operation through R and an explicit localized confirmation. Enter never
confirms. Its normal Esc exit code is not an operation result: inspect the
on-screen completion/error and re-query status before proceeding.

Only raw **no-rewind** aliases are accepted, including custom `-tX` aliases
and an optional trailing `$`, e.g. `NRST0$`. Buffered names (`NMT0`, also
`NRT0` with custom `-tR`), rewind-on-close aliases, drive-prefixed paths and
ordinary file paths are rejected. A regular disk file named `NRMT0` is rejected
by the device-type check before any control request. No file is created or
truncated, and the command never opens tape for data writing.

The command checks its complete syntax before opening a handle. It then opens
read-only, checks the DOS character-device flag, queries the guarded 16-byte
status ABI (or the supported 14-byte fallback), and requires unbuffered mode
with neither rewind-on-close nor written/write-error state. It sends exactly
one six-byte `mtop` request: `operation=MTREW`, `count=1` (count is ignored for
rewind by ASPITAP). There is no user-supplied operation or count and no retry.
The status-only `ot_probe_ops` interface retains no control callback; rewind
uses a separate `ot_rewind_ops` interface.

Success requires another valid status response with no pending rewind,
write-error state, extended driver error or SCSI error, and the last SCSI
opcode must be REWIND. Closing the handle must succeed too. An earlier failure
is not hidden by a later close failure. An initially pending rewind may be
handled by ASPITAP's readiness barrier; OpenTape adds no independent polling
loop. Driver/controller timeouts still apply, so physical rewinds may take a
while. No automatic retry is performed after an ambiguous failure; inspect the
drive before choosing another action. A close or console error can report
failure even if the drive has already rewound.

Exit `0` means checked synchronous completion; malformed syntax/device names
return `1`, and open/type/status/control/close/output errors return `2`.
Pre-command warning output is flushed before moving tape; if that flush fails,
no device is opened. Console completion is checked as well. Legacy status
cannot report the extended driver's error byte; its existing ABI limits remain.

## Listing, selection and restore

Listing/export and restore continue to start at the **current position**, with
no implicit rewind. For a standalone native archive starting at beginning of
tape, the sequence is:

```text
OPENTAPE --rewind --device NRMT0
OPENTAPE --list --device NRMT0 --selection-out FILES.LST
REM Review/edit FILES.LST to retain only authorized replacements.
OPENTAPE --rewind --device NRMT0
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --replace-list FILES.LST
```

Stop on any failing command. The listing reads through the first filemark;
rewind does not search for a later archive or select a session. Arbitrary
filemark/record spacing and whole-volume restore remain unimplemented.

## Tests and limits

Host `test_tape_probe` and DOS `PRBTEST.EXE` share 22 rewind fixtures, including
open/type/status failures, refusal before control, exact request layout,
single-attempt control failure, bad post-status/ABI/opcode, pending rewind,
legacy fallback, close failure and preservation of the first error. Invalid
callbacks/arguments and buffered custom `-tR` names must not open a device.
The original read-only probe fixtures still run.

`dos-catalog-smoke` first reads/exports the approved virtual native archive,
then performs two explicit rewinds (explicit/default unit) and verifies all
95 blocks after each rewind for the large fixture. Both operations must report
checked completion. Invalid options/names and a device-like regular file are
refused. Only the identity-guarded OA2101L8 virtual cartridge is used; data is
not written and no changer operation is issued. This does not qualify physical
drive timing, power failures, concurrent access or other drivers' semantics.

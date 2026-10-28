# Catalogue files in DOS batch commands

The command line uses the same [metadata codec and guarded file services](catalog-files.md)
as F8/F9. These commands write or read **catalogue metadata**, not backup data.

```text
OPENTAPE --list --catalog-out BACKUP.CAT
OPENTAPE --list --catalog-in BACKUP.CAT
OPENTAPE --ui --catalog-in BACKUP.CAT
```

## Export from tape

`--list --catalog-out FILE.CAT [--device NRMT0]` reads from the selected raw
no-rewind alias's **current position** through the first filemark. It never
rewinds automatically. Position the tape explicitly if needed. The native
transport accepts the existing prefix:

```text
OPENTAPE --native-rewind --device NRMT0
OPENTAPE --native --list --device NRMT0 --catalog-out BACKUP.CAT
OPENTAPE --native --fixed-block 512 --list --catalog-out BACKUP.CAT
```

Choose the existing backend/fixed-block setting appropriate to the device.
The normal command without `--native` uses ASPITAPE. Fixed/native transport
selection does not alter the on-disk catalogue format.

Before any tape I/O, the export checks that the target and its same-directory
`OT$LIST.TMP` are absent and the parent exists. It collects at most 256 entries
while the shared archive verifier checks framing, order, file contents/CRCs,
completion/filemark and tape close. No metadata rows or destination file are
published on a failed tape verification, including a late failure after entries
were collected. The initial current-position warning can still have appeared.

After successful tape close, the complete metadata listing is printed and
flushed. Only then does the shared catalogue save service recheck the output
paths, exclusively create staging, stream, close, independently read back and
publish the file. No device retry, tape write or extraction occurs. Existing
files and staging are refused. A failure during staging may leave the temporary
file for inspection.

The list includes directories, session/object IDs and full paths, including
long archive paths. Date/time and attribute columns use the existing live-list
format. The catalogue is bounded even if the tape is larger. Neither this file
nor its metadata CRC grants later restore/replacement permission.

## Read a catalogue offline

`--list --catalog-in FILE.CAT` checks the entire regular file, source snapshot,
exact EOF and close before printing any metadata rows. The heading explicitly
says **OFFLINE**, followed by the metadata columns, entries and total count.
An empty catalogue succeeds with zero entries. This checks the metadata file;
it does not reread file contents from tape or establish cartridge identity.
No tape is opened, probed, enumerated, rewound or read. A corrupt file produces
no apparently complete metadata listing. The input remains unchanged.

## Open directly in the DOS interface

`--ui --catalog-in FILE.CAT` explicitly loads the file before initializing the
screen and starts in the offline catalogue view. There is no startup dialog to
confirm an already requested read. Missing or corrupt input returns status 2
on the console without entering the interface. Invalid command syntax returns
status 1 before opening anything. There is still no automatic file discovery.

The initial marks and replacement grants are empty, and the opened filename is
remembered for the F8/F9 editor. Browsing, search and selection behave exactly
as for F9. C/W/K/O remain blocked until a fresh live catalogue has been read.
UI destination/source options still configure future operations; an explicit
UI `--device` only chooses that alias, without binding it to saved metadata.

## Syntax and exit status

Filenames have the same 40-character limit and regular 8.3 leaf checks as F8/F9.
Tape and DOS character-device aliases, wildcards and empty names are rejected
before open. Each option may occur only once and requires a value.

* `--catalog-in` and `--catalog-out` are mutually exclusive.
* In `--list`, neither catalogue option combines with `--selection-out`.
* Offline `--list --catalog-in` does not accept `--device`.
* `--catalog-out` is only accepted with `--list`; UI export still uses F8.
* `--catalog-in` is accepted only with `--list` or `--ui`; it cannot drive
  `--restore`, `--compare` or tape verification.

Status 0 means the requested operation completed. Status 1 means invalid
syntax/arguments. Status 2 means a filesystem, tape, metadata or output error.
Broken stdout before tape acquisition prevents acquisition; failed listing
output prevents publication. If stdout fails while printing the final message
**after** successful publication, status 2 can accompany an already valid
catalogue file. Inspect that file; export will refuse to overwrite it.

The existing plain `--list` / `--list --selection-out` workflows retain their
provisional-row and completion/error reporting rules.

## Verification

`test_catalog_cli` injects failed target preflight, metadata loading, late tape
verification, stdout writes/flushes and publication, checking operation order
and cleanup. Successful offline loading cannot call tape or output services.
The tests distinguish failure before publication from final-message failure.

`dos-catalog-cli-smoke` runs ten isolated DOS/QEMU cases for each language:
native/legacy/fixed live exports, offline and empty listings, invalid option
combinations and corrupt/missing input, protected targets/staging, tape CRC
failure, a 256-entry file above 64 KiB, and direct offline UI startup. Counters
check exact CDB totals (19 for native/legacy, 35 fixed, 12 corrupt, zero for
offline/invalid/staging/startup). Python independently decodes exported files;
read-only/error cases compare the complete private floppy image. No physical
media or library changer is involved.

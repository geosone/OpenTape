# OpenTape

OpenTape is a DOS tape backup, restore, verify, catalogue, and media-control
program.  It is a separate project from OpenASPI: users should not have to
replace a working tape driver merely to run the backup application.

The native OpenTape API is being built to provide common tape operations, capabilities
and results for DOS, Windows 3.x and Windows 9x/ME, with NT+ as a long-term
backend target. The existing ASPITAPE-compatible backend is one separate
compatibility provider; it does not define the native API's feature set.
The first [native DOS operations](docs/native-tape-api.md) provide status,
readiness and synchronous rewind. A native variable-record reader is
implemented beneath the frontend, together with a native writer using explicit
flush, filemarks, finish and abort. [Archive commands](docs/native-archive.md)
can select this backend with `--native`; `--native --ui` selects it in the
shared QUILIB interface.
Direct ASPI access is another planned backend path.

OpenTape's primary backup format is a self-describing sequential volume format
in the style of Bacula/Bareos. It works on one ordinary tape partition,
including DDS/DAT, and permits rebuilding a lost external catalogue by scanning
the media. LTFS remains a later optional backend for supported drives. See
[docs/tape-formats.md](docs/tape-formats.md) and
[docs/native-volume-format.md](docs/native-volume-format.md).

This repository is at the bootstrap stage. It provides DOS and wxWindows
Win16 start screens, read-only DOS tape status and raw data reading, the
native-format codec and host-side tests. An experimental DOS
[single-file backup command](docs/single-file-backup.md) and bounded
[directory-tree backup](docs/tree-backup.md), plus
[restore with per-file replacement selection](docs/restore.md), are available.
[Exact-file restore filters](docs/selective-restore.md) restrict extraction
separately from replacement permissions.
The [restore preview](docs/restore-preview.md) checks planned actions without
changing the destination.
The [archive-to-disk comparison](docs/archive-compare.md) checks existing file
contents without restoring them.

Under DOS, `OPENTAPE --status` queries `NRMT0` without moving or writing the
tape; `--status --device NRMT1` selects another no-rewind alias.
[`OPENTAPE --devices`](docs/device-enumeration.md) lists installed raw aliases.
See the
[legacy backend](backends/legacy_tape/README.md) and [testing guide](docs/testing.md).
`OPENTAPE --read-test` reads up to 16 blocks from the current position and
prints byte counts/CRC32 values until a filemark. It moves the tape by reading,
but does not rewind, write or restore files. This is a transport diagnostic,
not yet native-backup verification.
`OPENTAPE --scan` instead checks native block/record framing and displays the
volume label. It requires the volume beginning at the current position;
this framing-only command does not verify or restore files.
`OPENTAPE --verify` additionally checks sequential file/session ordering,
whole-file CRCs and completion totals. Add `--all` to read beyond the default
16-block diagnostic limit, through the first filemark. Neither mode verifies
a complete volume or restore. See
[verification limits](docs/archive-verification.md). `OPENTAPE --list` uses the
same verifier to display metadata and paths; see
[archive contents and checked path-list export](docs/archive-listing.md).
Explicit [DOS rewind](docs/tape-positioning.md) repositions the selected tape
between listing and restore. The shared native writer
now generates and validates session blocks through a sink interface. A guarded
[DOS output transport](docs/tape-output.md) has a guarded virtual-tape
write/readback test and guarded file/tree CLI commands; physical-drive qualification,
unrestricted selection and whole-volume recovery remain pending. See
[writer scope](docs/archive-writer.md).

The [DOS device/archive interface](docs/dos-interface.md) directly uses Open386MAX's
QUILIB. No private window,
dialog, keyboard, mouse, or colour implementation is kept in this repository.
The GeosOne DOS Installer (GDI) is reused for distribution packages. The
planned wxWindows Win16 frontend shares the same application core and runs on
Windows 3.1 as well as Windows 95/98/ME.

## Build

Open386MAX, OpenASPI, GDI, and wxWindows are pinned Git submodules below
`deps/`. Clone with `git clone --recursive`, or initialize an existing checkout
with `git submodule update --init --recursive`.
OpenWatcom is used for the DOS executable, Open386MAX supplies QUILIB and its
DOS build rules, and the host C compiler runs unit tests:

```sh
make
make test
make check
make win16
make dist
make languages
```

Like Open386MAX, OpenTape uses `MAXLANG=US` for English (the default) and
`MAXLANG=GR` for German. Builds are kept apart below `build/US` and `build/GR`;
`make languages` builds both localized GDI distributions. Override
`OPEN386MAX`, `OPENASPI`, `GDI`, or `WXWINDOWS` for another checkout layout.

See [docs/ui-language.md](docs/ui-language.md) for the common DOS and Windows
interaction language.

## Backend policy

1. OpenASPI extensions, when explicitly detected;
2. the portable ASPITAPE-compatible `RMTx`/`NRMTx` interface;
3. direct ASPI access as a fallback for devices without a character driver.

These are the current backend selection priorities. The native API is being
designed above the providers, with shared operations and explicit supported,
unsupported and unknown capabilities. Platform calling conventions, controller
ownership and device quirks belong inside the providers. The existing legacy
backend remains available for historical drivers with its actual feature
limits; no new native platform backend is claimed implemented by this policy.

## Tape-format policy

- Use the native self-describing format from the first backup implementation.
- Keep a fast external catalogue, but make every volume independently
  scannable so catalogue loss never makes the tape unreadable.
- Use one sequential partition, portable block/record serialization, checksums
  and explicit session/file/volume records.
- Add LTFS later as an optional format on compatible drives.
- Treat TAR as a future import/export compatibility path, not the native format.
- Require an explicit destructive confirmation before formatting any tape.
- Never require OpenTape's catalogue database to recover native media.

## License

OpenTape is licensed under the GNU General Public License version 3.  See
`LICENSE`.

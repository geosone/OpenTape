# OpenTape

OpenTape is a DOS tape backup, restore, verify, catalogue, and media-control
program.  It is a separate project from OpenASPI: users should not have to
replace a working tape driver merely to run the backup application.

The first compatibility target is the documented character-device interface
of Greg Shenaut's historical `ASPITAPE.SYS`.  The same legacy backend is
intended to work with OpenASPI's compatible `ASPITAP.SYS`.  Optional OpenASPI
extensions and direct ASPI access are separate backends.

OpenTape's primary backup format is a self-describing sequential volume format
in the style of Bacula/Bareos. It works on one ordinary tape partition,
including DDS/DAT, and permits rebuilding a lost external catalogue by scanning
the media. LTFS remains a later optional backend for supported drives. See
[docs/tape-formats.md](docs/tape-formats.md) and
[docs/native-volume-format.md](docs/native-volume-format.md).

This repository is at the bootstrap stage. It provides DOS and wxWindows
Win16 start screens, read-only DOS tape status and raw data reading, the
native-format codec and host-side tests. Backup/restore is not implemented yet.

Under DOS, `OPENTAPE --status` queries `NRMT0` without moving or writing the
tape; `--status --device NRMT1` selects another no-rewind alias. See the
[legacy backend](backends/legacy_tape/README.md) and [testing guide](docs/testing.md).
`OPENTAPE --read-test` reads up to 16 blocks from the current position and
prints byte counts/CRC32 values until a filemark. It moves the tape by reading,
but does not rewind, write or restore files. This is a transport diagnostic,
not yet native-backup verification.
`OPENTAPE --scan` instead checks native block/record framing and displays the
volume label. It requires the volume beginning at the current position;
complete backup verification and restore remain later steps.
File/session payload codecs and their initial DOS-8.3 schemas are now defined;
session/file ordering checks and the actual writer remain to be implemented.

The DOS interface directly uses Open386MAX's QUILIB.  No private window,
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

The legacy backend is the interoperability baseline.  OpenTape must remain
usable with the historical driver even after OpenASPI-specific diagnostics are
added.

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

# OpenTape

OpenTape is a DOS tape backup, restore, verify, catalogue, and media-control
program.  It is a separate project from OpenASPI: users should not have to
replace a working tape driver merely to run the backup application.

The first compatibility target is the documented character-device interface
of Greg Shenaut's historical `ASPITAPE.SYS`.  The same legacy backend is
intended to work with OpenASPI's compatible `ASPITAP.SYS`.  Optional OpenASPI
extensions and direct ASPI access are separate backends.

OpenTape writes standard LTFS on supported partition-capable tape generations.
DDS/DAT and other drives without LTFS-compatible partition support use
standard streaming TAR instead;
OpenTape will not create a private pseudo-LTFS variant that other software
cannot read. See [docs/tape-formats.md](docs/tape-formats.md).

This repository is at the bootstrap stage. It currently provides buildable
DOS and wxWindows Win16 start screens, the backend-selection contract, the
shared OpenASPI ASPITAPE definitions, and host-side tests. It does not read or
write tapes yet.

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

- Detect an existing LTFS or TAR volume without changing it.
- Offer LTFS only when the drive and cartridge expose the required partition
  capabilities through OpenASPI extensions or direct ASPI.
- Use streaming TAR for DDS/DAT and other non-partitioning drives.
- Require an explicit destructive confirmation before formatting any tape.
- Never require OpenTape's catalogue database to restore standard media.

## License

OpenTape is licensed under the GNU General Public License version 3.  See
`LICENSE`.

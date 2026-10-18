# OpenTape

OpenTape is a DOS tape backup, restore, verify, catalogue, and media-control
program.  It is a separate project from OpenASPI: users should not have to
replace a working tape driver merely to run the backup application.

The first compatibility target is the documented character-device interface
of Greg Shenaut's historical `ASPITAPE.SYS`.  The same legacy backend is
intended to work with OpenASPI's compatible `ASPITAP.SYS`.  Optional OpenASPI
extensions and direct ASPI access are separate backends.

This repository is at the bootstrap stage.  It currently provides a buildable
16-bit DOS executable, the backend-selection contract, the shared OpenASPI
ASPITAPE definitions, and host-side tests.  It does not read or write tapes
yet.

The DOS interface directly uses Open386MAX's QUILIB.  No private window,
dialog, keyboard, mouse, or colour implementation is kept in this repository.
The GeosOne DOS Installer (GDI) is reused for distribution packages. The
planned wxWindows Win16 frontend shares the same application core and runs on
Windows 3.1 as well as Windows 95/98/ME.

## Build

The default sibling checkouts are `../386MAX`, `../OpenAspi`, and `../gdi`.
OpenWatcom is used for the DOS executable, Open386MAX supplies QUILIB and its
DOS build rules, and the host C compiler runs unit tests:

```sh
make
make test
make check
make dist
```

The DOS build is written to `build/dos/OPENTAPE.EXE`. Override
`OPEN386MAX`, `OPENASPI`, or `GDI` for another checkout layout. `make dist`
creates a GDI-based installation directory below `build/dist`.

See [docs/ui-language.md](docs/ui-language.md) for the common DOS and Windows
interaction language.

## Backend policy

1. OpenASPI extensions, when explicitly detected;
2. the portable ASPITAPE-compatible `RMTx`/`NRMTx` interface;
3. direct ASPI access as a fallback for devices without a character driver.

The legacy backend is the interoperability baseline.  OpenTape must remain
usable with the historical driver even after OpenASPI-specific diagnostics are
added.

## License

OpenTape is licensed under the GNU General Public License version 3.  See
`LICENSE`.

# Architecture

```text
DOS QUILIB / wxWindows Win16 / command line
        |
backup / restore / verify / catalogue
        |
tape abstraction
   |         |          |
legacy   OpenASPI   direct ASPI
RMTx     extensions    SRBs
```

The tape abstraction owns positioning, media state, fixed/variable block
selection, streaming reads and writes, filemarks, and normalized errors.  The
archive layer must not issue SCSI commands or DOS IOCTLs directly.

Frontends consume one application-service API and contain no archive or tape
policy. The DOS frontend consumes Open386MAX QUILIB rather than forking it.
The Windows frontend reuses the existing wxWindows 1.68E Win16 port. One
binary therefore supports Windows 3.1 and Windows 95/98/ME without a second
application UI.

Package installation is described by GDI `INSTALL.INI`, not implemented in
OpenTape.

The initial archive format target is streaming TAR with an OpenTape metadata
extension only where standard TAR cannot represent required DOS information.
Restore must remain possible without the catalogue database.

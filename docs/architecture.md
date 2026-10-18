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

The initial format layer writes OpenTape's native sequential volume format. A
volume contains a label, checksummed blocks, typed records and explicit session
boundaries on one ordinary tape partition. The external catalogue accelerates
selection and positioning, but a scanner can reconstruct it from the volume.
This path works through the legacy `RMTx`/`NRMTx` interface and therefore covers
DDS/DAT as well as newer drives.

LTFS remains a later optional backend for drives and cartridges with the
required two-partition capability. It needs operations not present in the
historical ASPITAPE API and therefore requires detected OpenASPI extensions or
direct ASPI. TAR is reserved for a later import/export compatibility path.

# Roadmap

1. [partial] Query a selected no-rewind device and retrieve `mtget` without
   changing media (implemented, including the OpenASPI 16-byte extension;
   automatic enumeration remains).
2. Implement rewind, status, load/unload and spacing in a small CLI tool.
3. [partial] Raw read sessions and bounded `--read-test` with short-record,
   filemark and fixed-block alignment handling are implemented. DOS/mhVTL
   variable records are tested; real fixed-block devices remain unqualified.
4. Add write support behind explicit write-protection and confirmation checks.
5. Implement native volume label, block/record streaming, catalogue rebuild,
   backup, restore and verify without a TUI.
6. Add partition capability probing and standard LTFS for supported LTO and
   IBM enterprise generations; add TAR import/export separately.
7. Build the 80x25 DOS screens and persistent catalogue on Open386MAX QUILIB.
8. Add one wxWindows Win16 frontend for Windows 3.1 and Windows 95/98/ME.
9. Add direct-ASPI fallback and optional OpenASPI diagnostics.

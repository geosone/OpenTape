# Roadmap

1. [partial] Query a selected no-rewind device and retrieve `mtget` without
   changing media (implemented, including the OpenASPI 16-byte extension;
   `--devices` now enumerates the four supported raw aliases in an explicitly
   configured letter/dollar namespace; controller/direct-ASPI discovery remains).
2. [partial] Status and explicit synchronous `--rewind` are implemented with
   guarded ABI/status/close checks. Native signed SPACE records/filemarks and
   EOD now retain uncertain state on error and require explicit rewind recovery;
   synchronous native LOAD/UNLOAD now support media recovery while preserving
   earlier uncertainty; absolute positioning remains pending.
3. [partial] Raw read sessions and bounded `--read-test` with short-record,
   filemark and fixed-block alignment handling are implemented. DOS/mhVTL
   variable records are tested; real fixed-block devices remain unqualified.
4. [partial] Single-session DOS output transport with explicit authorization,
   exact-write checks and post-close error checks is implemented. Command-level
   confirmation and media identity/protection policy remain. A separately gated
   native session write/readback test now covers the approved virtual scratch tape.
5. Implement native volume label, block/record streaming, catalogue rebuild,
   backup, restore and verify without a TUI.
   Native framing/label scanning and file/session payload codecs are implemented.
   Sequential ordering/completion checks and whole-file CRC verification are
   implemented. The native writer core and memory-sink roundtrips are implemented;
   the device sink has virtual-tape write/readback coverage; physical qualification,
   recovery and catalogue rebuild remain pending. Single-file DOS
   source streaming and an explicitly authorized CLI are now implemented;
   bounded tree planning/streaming includes multiple files and empty directories.
   Unrestricted multi-root selection and atomic changed-source handling remain.
   `--verify --all` now streams beyond the default diagnostic limit through the
   first filemark; whole-volume/multi-filemark verification remains pending.
   Restore uses exclusive staging, archive CRC and staged disk readback before
   publication. Existing roots and
   replacement require exact per-file selections; other existing files stay
   unchanged. General recovery, graphical selection and directory metadata replay remain.
   Replacement-list input now combines up to 256 exact names before archive I/O,
   avoiding DOS command-line length and the former 16-name selection mask.
   `--list` now exposes provisional metadata through the shared verifier and
   confirms listing completion at the first filemark; persistent catalogue rebuild remains.
   `--selection-out` exports checked regular-file paths for a reviewed replacement
   list, publishing only after verification and exact staged writes/close succeed.
   `--only`/`--only-list` now restrict extraction to exact regular-file paths
   independently of replacement permissions; excluded data remains verified.
   `--dry-run` uses the same restore core with read-only destination inspection
   to preview actions without creating a root or staging files; writeability,
   free-space guarantees and persistent plans remain outside its scope.
   `--compare` now checks selected existing file lengths/CRCs against the archive
   without destination writes; metadata equality and full difference reports remain.
6. Add partition capability probing and standard LTFS for supported LTO and
   IBM enterprise generations; add TAR import/export separately.
7. [partial] The 80x25 DOS device/status screen now uses Open386MAX QUILIB
   colors, windows and input, with explicit alias selection, read-only queries
   and separately confirmed synchronous rewind and archive verification through
   the first filemark using the shared CLI session runner.
   A bounded verified contents view now browses up to 256 entries with page,
   row and complete-path navigation, temporary per-file marks and all/none.
   Confirmed export now writes marked paths through the existing protected
   selection writer, refusing ambiguous versions and existing output files.
   Persistent catalogue, applying marks directly to restore
   and backup/restore dialogs remain pending.
8. Add one wxWindows Win16 frontend for Windows 3.1 and Windows 95/98/ME.
9. Add direct-ASPI fallback and optional OpenASPI diagnostics.

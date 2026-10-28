# Roadmap

1. [partial] Query a selected no-rewind device and retrieve `mtget` without
   changing media (implemented, including the OpenASPI 16-byte extension;
   `--devices` now enumerates the four supported raw aliases in an explicitly
   configured letter/dollar namespace; controller/direct-ASPI discovery remains).
2. [partial] Status and explicit synchronous `--rewind` are implemented with
   guarded ABI/status/close checks. Native signed SPACE records/filemarks and
   EOD now retain uncertain state on error and require explicit rewind recovery;
   synchronous native LOAD/UNLOAD now support media recovery while preserving
   earlier uncertainty. Native LOCK/UNLOCK now control cartridge removal with
   non-moving failure semantics and preserved legacy EOF; absolute positioning
   with LOCATE(10) now supports current/explicit partition selection and checked
   32-bit addresses; failed moves retain uncertainty with no replay or emulation.
   Short logical READ POSITION observes
   full-width coordinates with explicit validity/length provenance and preserves
   session uncertainty and legacy stream state. Long READ POSITION and
   LOCATE(16) now retain full 64-bit coordinates and independent filemark validity
   through checked DOS decimal conversion; old format-0 bindings remain available.
3. [partial] Raw read sessions and bounded `--read-test` with short-record,
   filemark and fixed-block alignment handling are implemented. DOS/mhVTL
   variable records are tested; real fixed-block devices remain unqualified.
   Native READ now identifies current 00/03 setmarks separately from filemarks,
   preserves count provenance and prevents a setmark from completing OTB1
   verification. Explicit setmark writing now uses the owned native writer,
   with independent capability gating and retained dirty/failed state.
   Explicit TapeAlert retrieval now reports known/active flags through the
   driver, SDK, CLI and confirmed DOS view; health policy/history remain open.
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
   recovery and catalogue rebuild spanning multiple media remain pending. Single-file DOS
   source streaming and an explicitly authorized CLI are now implemented;
   bounded tree planning/streaming includes multiple files and empty directories.
   Bounded [backup-set manifests](backup-sets.md) now combine file/tree roots
   through the existing planner and writer. Unrestricted multi-root selection
   and whole-set changed-source handling remain.
   `--verify --all` now streams beyond the default diagnostic limit through the
   first filemark; whole-volume/multi-filemark verification remains pending.
   Restore uses exclusive staging, archive CRC and staged disk readback before
   publication. Existing roots and
   replacement require exact per-file selections; other existing files stay
   unchanged. General recovery and directory metadata replay remain.
   Replacement-list input now combines up to 256 exact names before archive I/O,
   avoiding DOS command-line length and the former 16-name selection mask.
   `--list` now exposes provisional metadata through the shared verifier and
   confirms listing completion at the first filemark; recovery and persistent
   indexes spanning multiple media remain.
   `--selection-out` exports checked regular-file paths for a reviewed replacement
   list, publishing only after verification and exact staged writes/close succeed.
   `--only`/`--only-list` now restrict extraction to exact regular-file paths
   independently of replacement permissions; excluded data remains verified.
   `--dry-run` uses the same restore core with read-only destination inspection
   to preview actions without creating a root or staging files; writeability,
   free-space guarantees and persistent plans remain outside its scope.
   The DOS catalogue now offers confirmed C preview of copied marked paths,
   reusing this core without destination writes or replacement permission;
   W restores marked paths through the same staging/readback callbacks. O now
   builds separate per-file replacement grants; preview reports them, W confirms
   and consumes them, and other existing files remain protected.
   `--compare` now checks selected existing file lengths/CRCs against the archive
   without destination writes; metadata equality and full difference reports remain.
6. [partial] Read-only reported partition page data now has a checked SDK,
   CLI and native DOS controls view. Pending/stale reports do not establish
   committed layout or valid targets. Add qualified partition preparation and standard LTFS for supported LTO and
   IBM enterprise generations; add TAR import/export separately.
7. [partial] Mouse input now uses the existing QUILIB INT 33h foundation with
   current-view hit maps, release activation and independent confirmation controls
   across device/catalogue/native/backup views; keyboard input remains available.
   The 80x25 DOS device/status screen uses Open386MAX QUILIB
   colors, windows and input, with explicit alias selection, read-only queries
   and separately confirmed synchronous rewind and archive verification through
   the first filemark using the shared CLI session runner.
   A bounded verified contents view now browses up to 256 entries with page,
   row and complete-path navigation, temporary per-file marks and all/none.
   Confirmed export now writes marked paths through the existing protected
   selection writer, refusing ambiguous versions and existing output files.
   The native T subview now exposes checked 64-bit position/LOCATE, load/unload,
   removal lock/unlock and explicit rewind, with bounded target editing,
   confirmation, retained failure state and the same QUILIB colors.
   The S chooser now adds signed relative record/filemark spacing and EOD,
   with shared CLI/UI count parsing and confirmed movement. Entries 4/5/6
   extend the same path to consecutive filemarks and individual/consecutive
   setmarks; unsupported hardware is reported without automatic mode changes.
   W now applies copied marks to a separately confirmed restore into a new
   destination, including staging readback and retained partial-failure diagnostics.
   O now grants exact replacements for marked files, with a separate copied subset,
   preview counts and one-batch write permission. I now imports extraction lists
   through the shared bounded parser and atomic catalogue resolver, clearing
   replacement grants only on success. B now offers the shared file/tree backup runner with one-shot current-position
   write confirmation and retained failure diagnostics; independent readback is
   still explicit. F now edits source/path/volume and file/tree mode with the
   shared QUILIB editor, transactional F2/Esc and mouse focus/actions; saving
   grants no tape write. Bounded [backup sets](backup-sets.md) now configure multi-root manifests
   through Set mode, with shared preflight, progress and version-2 profiles. P edits independent preview/restore/compare destinations and
   selection input/output paths with the same bounded QUILIB controls; settings
   preserve marks/grants and still require separate operation confirmation.
   S searches complete catalogue paths offline; F3/Shift-F3 navigates hits with
   wraparound, preserving all marks/replacement intent and issuing no I/O.
   Enter now toggles a directory's regular descendants within its session,
   respecting path boundaries and retaining/revoking exact grants through the
   shared mark rules. Directories themselves remain unmarked.
   F5/F6 now export/import versioned, CRC-checked configuration profiles with
   staged disk readback and all-or-nothing import; no device state or grants
   are persisted, no profile is loaded automatically and existing exports stay.
   F8/F9 now persist versioned metadata catalogues with staged readback and
   atomic import. Imported files are explicitly offline; C/W/K/O require a fresh
   live catalogue and no marks/grants or tape identity are serialized. Recovery
   and persistent indexes spanning multiple media remain pending. See
   [catalogue files](catalog-files.md). The [batch interface](catalog-cli.md)
   now exports with `--list --catalog-out`, lists saved metadata with
   `--list --catalog-in`, and opens the offline UI with `--ui --catalog-in`.
   Live file publication follows successful tape close and flushed listing;
   offline input never probes or associates a tape.
   F7 now provides read-only source preflight with file/directory totals and
   checked wide payload accounting, reusing the bounded tree planner. No tape
   state or write grant changes; B still plans afresh after confirmation.
   Backup now reports live accepted-record progress, full archive paths and
   completed-file/payload counters through a read-only writer observer. Counts
   remain provisional through finalization and no extra device commands occur.
   V/L now show [live block verification progress](verification-progress.md),
   including paths and aggregate counts across sessions. Observation follows
   complete block/visitor success; filemark/close still determines completion.
   C/W/K now show committed-block progress with full current/last paths,
   kept/excluded counts and cumulative checked payload across sessions. Final
   success still requires filemark/coverage/close; observations cannot grant I/O.
   K now compares selected disk file
   contents through the shared read-only backend, with confirmed rewind/read,
   exact size/CRC/EOF checks and retained replacement intent.
8. Add one wxWindows Win16 frontend for Windows 3.1 and Windows 95/98/ME.
9. Add direct-ASPI fallback and optional OpenASPI diagnostics.

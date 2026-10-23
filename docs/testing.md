# Testing OpenTape

`make MAXLANG=US check` and `make MAXLANG=GR check` build the DOS and Win16
frontends and run host tests for backend selection, the native-format codec
and read-only tape probing/reading. Probe fixtures cover modern/legacy status,
normalization, invalid/rewinding names, missing devices, ordinary files,
guard corruption, detected incompatible legacy layouts and cleanup/error
precedence. No host unit test accesses a tape.
Reader tests cover raw-mode setup, short data, filemarks and continuation,
fixed-block alignment, invalid counts, failed-state/no-retry behavior,
diagnostic failure precedence and cleanup. They also pass with AddressSanitizer
and UndefinedBehaviorSanitizer on the host.
Native-scanner tests cover valid labels and unknown records, every truncated
prefix of a label block, block/fragment CRC errors, record-count/length and
sequence mismatches, duplicate/bad labels, invalid fragment offsets and counter
overflow. A host-only test validates a full 65,535-byte block with outer guards.
Native scanner tests also pass with both host sanitizers.
Typed-payload tests cover little-endian reference bytes, session/file start/end
roundtrips, DOS attributes and calendar dates, path length boundaries, truncated
metadata, reserved fields and device/traversal names. The scanner must reject
malicious typed payloads even when all block and fragment CRCs are valid, without
committing partial state. Both host sanitizers also cover these payload tests.
Archive-state tests additionally cover fragmented whole-file CRCs against the
standard `123456789` reference, empty files, directories, multiple sessions,
wrong IDs/times, premature endings, gaps/duplicate fragments, bad totals,
unsupported records and whole-block rollback. Counter overflow/carry cases
inject boundary state; they do not claim multi-gigabyte tape qualification.
The same state tests pass with both host sanitizers.
Writer tests use a memory sink and then feed the captured physical blocks into
a separate verifier instance. They cover known whole-file CRCs, empty files,
directories, multiple sessions, scratch-buffer aliasing, short/error/invalid-count
writes, delayed finalization errors and rejected reentrancy. A host-only guarded
test emits a 65,535-byte block; the host writer tests also pass with both sanitizers.

## DOS status integration

`make dos-status-smoke` boots the actual OpenTape DOS executable using a copy
of an externally prepared OpenASPI dc390/mhVTL boot image. Prepare that image
with OpenASPI's `dos-mhvtl-smoke` setup: it must load the matching external DOS
ASPI manager and `ASPITAP.SYS`, using `DELAY.SYS` for delayed target hotplug.
See [OpenASPI's setup guide](../deps/OpenASPI/docs/qemu-mhvtl.md).
No vendor manager or boot disk is redistributed. The template is copied,
not edited. The changer is only queried for identity; it is not passed to DOS.

For the currently verified local virtual setup:

```sh
DOS_BOOT_IMAGE=/home/mario/mars/OpenAspi/build/dos-mhvtl/boot.img \
MHVTL_TAPE=/dev/sg6 \
MHVTL_VERIFY_CHANGER=/dev/sg13 \
MHVTL_VERIFY_DRIVE=0 \
MHVTL_SCRATCH_BARCODE=OA2101L8 \
make MAXLANG=US dos-status-smoke
```

Repeat with `MAXLANG=GR`. These node and barcode values are local examples,
not portable defaults. The test deliberately reuses OpenASPI's strict mhVTL
identity gate even though the application only requests status. Physical
devices are refused. It performs no media changes, writes or motion commands.
Use a template whose no-rewind aliases follow the expected first-drive setup;
units 2 and 3 must not already be installed.

The batch checks help, default/explicit `NRMT0` status, rejection of `RMT0`,
an unavailable unit and a regular file named `NRMT3`. The file must remain
byte-identical. A checked DOS batch marker, expected ASPI address and QEMU
debug-exit status are required. Logs/images live under
`build/US/dos-status/` or `build/GR/dos-status/`. Do not run two tests for the
same language concurrently. No local filemark patch is needed for status.

The English and German DOS status tests passed on 2026-10-22 with the installed
unmodified QEMU 10.2.3, Tekram ASPI 1.60 and mhVTL's IBM ULT3580-TD8. Legacy
fallback is host-fixture tested, not yet qualified with the historical driver.
The Windows frontend is build-tested only; it does not use this DOS backend.

## DOS data-read integration

`make dos-read-smoke` extends the same integration harness with a bounded data
read. It requires the known final buffered-roundtrip layout written by the
current OpenASPI `dos-mhvtl-roundtrip` test on the approved disposable virtual
cartridge. That layout is three 1024-byte physical records containing 2,500
patterned bytes and 572 zero padding bytes, followed by filemarks. The read
test does not create or overwrite this fixture.

Use the same environment as above, but additionally select
`QEMU_BIN=/path/to/patched/qemu-system-i386` and call `make MAXLANG=US dos-read-smoke`
(repeat with `MAXLANG=GR`). The tested QEMU 10.2.3 filemark workaround is
described in OpenASPI's setup guide. Installed unmodified QEMU is suitable for
status only, not this filemark test.

The harness revalidates the virtual cartridge and explicitly rewinds it with
host `sg_raw` before booting; this moves the tape but writes no media data.
Only the read-test mode enables this motion. The application itself has no
rewind in `--read-test`. Wrong/buffered names and regular files must be rejected
before reading. Acceptance requires three 1024-byte records with CRC32 values
`227BE3D1`, `76A5CF1D`, `4D13C3E7`, then a filemark and a checked DOS exit.
These expected values come from the known transport fixture, not an OpenTape
backup. Do not interpret them as native-format compatibility or restore tests.

English and German read tests passed on 2026-10-22 through Tekram 1.60 and
mhVTL with the local QEMU patch. Logs are under `build/US/dos-read/` and
`build/GR/dos-read/`. The final tape position is after the first filemark;
no automatic cleanup rewind is performed. No physical tapes were tested.

## Native framing under DOS

All integration modes additionally run `NATTEST.EXE`, a DOS build of the same
native-scanner positive/negative fixtures (excluding the large host-only boundary
case). This test executable runs entirely in memory and is not distributed
as part of the user application. Its success marker and DOS exit are required.
They also run `PAYTEST.EXE`, a DOS build of the typed-payload tests, with a
separate required success marker/exit. These in-memory tests do not write tapes
or create restored files. English and German integration runs passed with both
test programs on 2026-10-22.
`VERTEST.EXE` now runs the archive-state fixtures under DOS as well, including
positive closed sessions and deliberately incomplete/corrupt sequences. Its
success marker and DOS exit are separately required. English/German runs passed.
`WRITEST.EXE` additionally runs the memory-sink writer/readback fixtures under
DOS, excluding the host-only maximum-block allocation. Its own marker and DOS
exit are required. `WRITEST.EXE` remains an in-memory test. Device output uses
the separately authorized `TAPETEST.EXE` mode described below.
Both English/German DOS runs passed on 2026-10-22 with the memory writer enabled.

`make dos-native-smoke` selects a separate integration mode using the same
environment and known non-native scratch fixture as the data-read test. After
identity verification it explicitly rewinds that virtual cartridge on the host.
The actual `OPENTAPE --scan` and `--verify` commands run in succession and must
each reject a non-native record with bad magic and exit `2`; raw driver-test
bytes must never be mistaken for an OpenTape volume. English/German tests
passed on 2026-10-22; logs are in
`build/US/dos-native/` and `build/GR/dos-native/`.

These rejection tests establish positive framing parsing in DOS memory and negative
format recognition through DOS ASPI/mhVTL. This is not backup/restore qualification.
The native rejection test leaves the tape after the second physical record;
the raw-read regression leaves it after the first filemark. No media is written.

## Native tape write/readback

`make dos-write-smoke` overwrites **only the approved disposable OA2101L8
mhVTL cartridge**. In addition to the device/boot-image variables above, it
requires `MHVTL_ALLOW_OVERWRITE=OA2101L8`. Never use valuable media. The harness
validates the cartridge/drive mapping and clear hardware WP bit twice before
write launch. It explicitly rewinds the verified virtual tape before each boot.

The first boot refuses an incomplete authorization command, then writes eleven
native records through the DOS output sink. The second boot compares all records
byte-for-byte, runs the independent state/CRC verifier and requires the next
read to return a filemark. The third boot runs `OPENTAPE --verify` and requires
eleven blocks/records and one closed session. See [transport scope](tape-output.md).
Logs are separated under `build/{US,GR}/dos-tape-{write,verify,cli-verify}/`.
Use the locally patched QEMU described above for zero-data filemark completion.
English and German write, byte-exact readback and CLI verification all passed
on 2026-10-22 using OA2101L8, `/dev/sg6`, changer `/dev/sg13`, drive 0.

**Fixture change:** this overwrites the old three-record OpenASPI roundtrip
fixture. `dos-read-smoke` and `dos-native-smoke` require that old fixture and
must not be run against the new native archive with their old expectations.
Status smoke remains valid; readback modes only rewind/read the native archive.
There is no automatic restoration of the old media content or final rewind.

This qualifies one virtual variable-block drive/session, not physical devices,
fixed blocks, real-media write protection, end-of-media, restore, source-file
traversal or general backup readiness. The diagnostic executable is not shipped
in distribution packages. The initial single-file CLI is tested separately below.

## Real DOS file backup

`make dos-file-smoke` uses the same device variables and mandatory
`MHVTL_ALLOW_OVERWRITE=OA2101L8` gate. It exercises the new `--backup-file`
command against a real DOS binary, a separate byte/metadata readback program,
and the normal `--verify` in three boots. Details and limitations are in
[single-file backup](single-file-backup.md). `SRCTEST.EXE` additionally runs
source-failure fixtures in memory in every integration mode.
This mode overwrites the previous fixed native fixture. Use only the approved
disposable virtual cartridge; no physical tape or changer operation is tested.
English and German runs passed on 2026-10-22: a 41,150-byte DOS binary was
backed up as seven native records, metadata/data readback matched, and normal
verification reported one closed session. Host tests, DOS source fixtures and
the source-streaming AddressSanitizer/UndefinedBehaviorSanitizer run also passed.

## DOS tree backup

`make dos-tree-smoke` uses the same exact OA2101L8 overwrite/identity/WP gates
and three-boot workflow. The fixture contains three files and three directories,
including an empty file/directory and duplicate basenames in different folders.
Readback compares selected paths, file metadata and bytes before requiring the
filemark; normal CLI verification checks the fifteen-record session.
`PLANTEST.EXE` runs the independent plan failure/limit fixtures under DOS in all
integration modes. See [tree scope and limits](tree-backup.md).
English/German write, tree-path/file-metadata/byte readback and CLI verification
passed on 2026-10-22 against OA2101L8: six entries, fifteen native blocks and
one closed session. The plan tests also passed with host ASan/UBSan.

## Verification beyond the diagnostic limit

`OPENTAPE_TREE_LARGE=1 make dos-tree-smoke` keeps all existing scratch-media
identity, WP and explicit overwrite gates. Its four boots write a 95-record
tree, compare all selected paths/file metadata/data bytes, require bounded
`--verify` to exit `3`, and require `--verify --all` to finish one session at
the filemark. Invalid `--all` combinations and a device-like regular file are
also refused before data reads. Logs live in the `dos-tape-tree-*-large`
directories. `STRMTEST.EXE` runs stream positive/error/limit fixtures in DOS
in every integration mode; the same tests run on the host and under ASan/UBSan.
The mode overwrites the previous small-tree fixture on OA2101L8. Small-fixture
readbacks require rewriting their fixture before reuse.
English and German extended runs passed on 2026-10-22: 46 entries, 95 blocks,
byte/metadata readback success, bounded exit `3`, and unbounded-through-filemark
exit `0` with one closed session. Only the virtual scratch cartridge was written.

## Actual DOS extraction

`make dos-restore-smoke` retains all exact OA2101L8 identity/WP/overwrite gates
and defaults to the 95-record tree fixture. It writes in the setup boot,
verifies in another, and actually restores into a new directory in a third.
`RSTCMP.EXE` compares every recovered path and file byte/metadata with the source;
existing targets (empty and populated) and a repeated restore are refused.
`RESTEST.EXE` additionally runs injected restore safety/failure fixtures under
DOS in all modes. See [restore safety and limits](restore.md). Restore itself
never writes tape; recovered files reside inside the cloned test boot image.
English/German full backup/verify/extract runs passed on 2026-10-22: 95 native
records, 43 recovered files and three directories; all 46 selected entries
matched their source paths/file bytes/time/date/attributes. Existing targets
were refused, both post-restore comparisons passed, and no staging file remained.
Host/DOS restore error fixtures and host ASan/UBSan also passed. Only setup
overwrote OA2101L8; physical media and unrelated host files were untouched.

## Per-file overwrite selection

`make dos-replace-smoke` uses the same exact OA2101L8 gates, runs normal new-root
restore regression, then restores into a seeded existing target using two exact
`--replace` paths. One selected old file is read-only/hidden/system; both must
match archived bytes/metadata afterward. A third existing file not selected must
retain its different contents/attributes, and an unrelated file must be unchanged.
Successful exchange must leave neither staging nor old-version temporary files.
Host/DOS fixtures additionally cover skipped corrupt input and every exchange
step failure, rollback failure and competing destinations. English/German runs
passed on 2026-10-22; host ASan/UBSan also passed. Replacement/deletion was
confined to deliberately seeded files in cloned DOS test images.

## Replacement-list input

`make dos-list-smoke` adds malformed/cross-input-duplicate list refusal and a
41-name list combined with one explicit name. All 42 seeded selected files
must be replaced; unselected and unrelated files stay unchanged. Normal
new-root restore regression also runs before the list test. `SELTEST.EXE` runs
the parser/source-failure/256-name fixtures under DOS in all integration modes;
the same suite runs on the host and under ASan/UBSan. See
[list format and preflight](restore-selection.md). Use the same approved
OA2101L8 setup authorization and device variables as the previous tests.
English/German list runs passed on 2026-10-22: 42 explicitly selected existing
files replaced, one unselected archived file plus an unrelated file preserved,
46 entries compared, and no staging/recovery files left after success. Malformed
and duplicate-input lists were refused before archive reads. Host/DOS parser
fixtures (including 256 names) and host ASan/UBSan passed as well.

## Read-only archive contents

`OPENTAPE_TREE_LARGE=1 make dos-catalog-smoke` reads the existing approved native
OA2101L8 fixture; it does not write tape or require overwrite authorization.
The harness verifies identity and explicitly rewinds that virtual fixture only.
Actual `--list` calls must show 43 file rows, three directory rows, expected
paths, 95 verified records and a completed-list footer. Invalid options, rewind
aliases and a device-like regular file are refused first. English/German runs
passed on 2026-10-22 (explicit/default device respectively).
`STRMTEST.EXE` and host stream tests additionally cover visitor output failure,
rejected metadata without a callback, and a missing terminator after all rows.
Host ASan/UBSan passed. Details are in [archive listing](archive-listing.md).

## Verified path-list export

`dos-catalog-smoke` also runs `--list --selection-out FILES.LST`, requires the
exact known set of 43 file paths (no directory or status lines), and refuses a
second export without changing the file. Invalid aliases must leave no output.
Existing destinations/directories, device names, missing parents, a reserved
staging destination and a pre-existing adjacent staging file are refused.
The pre-existing staging fixture remains byte-identical. Tests use the existing
approved cartridge read-only; no new tape write authorization is needed.

The host/DOS selection suite now tests 256-path and maximum-length-path
save/load round trips, empty lists, invalid entries, write errors, short writes,
over-reported counts, close errors and non-replacing publication failure.
Close is attempted once and publication never follows a write or close error.
See [export semantics and recovery limits](archive-listing.md).

English/German builds and read-only DOS export runs passed on 2026-10-22:
95 blocks verified, exactly 43 regular-file paths exported, three directories
excluded, existing output/staging fixtures preserved. Both language `check`
targets, host/DOS selection tests and host ASan/UBSan passed. These are virtual
scratch-tape tests, not physical-device or power-failure qualification.

## Explicit synchronous rewind

`dos-catalog-smoke` now continues after export: `--rewind --device NRMT0`, full
native verification, `--rewind` with the default device, and full verification
again. Each rewind must report checked completion; each subsequent verification
must see the original 95 blocks/95 records. The first listing and two subsequent
verifications therefore produce three complete framing results. The harness
still validates cartridge identity before launch; no tape data is written and
no changer command is used. Invalid options, buffered names (including custom
`NRT0$`), rewind-on-close aliases and a regular file masquerading as an alias
are refused before valid tape operations. The regular-file fixture remains
byte-identical.

`PRBTEST.EXE` is included in every DOS smoke boot. It runs the read-only status
fixtures and 22 synchronous-rewind fixtures shared with the host probe suite.
Those cover request layout, guarded status/fallback, pre-control refusal,
control/post-status/close failures and first-error preservation. Host ASan/UBSan
also runs the same fixtures. See [rewind scope and workflow](tape-positioning.md).

English/German `check` builds and DOS rewind/export runs passed on 2026-10-22.
Each language run completed two explicit rewinds and reverified all 95 blocks
after each; the 43-path export and existing-file preservation checks still
passed. Host ASan/UBSan passed the shared probe/rewind fixtures. No physical
drive or concurrent-access qualification is implied.

## Selective restore

`make dos-filter-smoke` reads the existing large OA2101L8 native fixture without
writing tape. Empty, malformed and duplicate filters plus replacement names
outside a filter fail before destination creation. A first restore creates only
two selected empty files and no unrelated archive directories/files. Following
explicit rewind, a second restores one authorized replacement, preserves one
selected existing file without replacement permission and excludes 41 files.
An excluded nonempty read-only/hidden/system file retains bytes/attributes and
an unrelated file is unchanged. A third pass fails at the filemark on an
unmatched name. Target staging/recovery files must be absent after these cases.
Artifacts are under `build/{US,GR}/dos-tape-tree-filter-large/`.

Host/DOS restore fixtures additionally exercise selected nonempty data, empty
files, case matching, multiple sessions, absent/directory-only names, skipped
selected files, corruption/duplicate/reserved names in excluded entries,
incomplete sessions, filter configuration guards and index 255. Existing
full-restore and exchange-failure fixtures run as regressions. See
[filter semantics and limits](selective-restore.md).

English/German `check` builds and selective DOS runs passed on 2026-10-22.
Each successful filtered pass excluded 41 files; two new selected empty files
were created, then one selected replacement was applied while another selected
existing file and excluded/unrelated files were preserved. Missing-name failure
was detected at the filemark. Host ASan/UBSan passed the shared restore suite.
These tests used the existing virtual archive without any tape data writes.
English/German read-only catalogue/export/rewind smoke regressions also passed
with the same 95-record archive after the filter changes.

## Read-only restore preview

`make dos-preview-smoke` performs a full 43-file/three-directory preview into
an absent root, then a filtered preview with one planned replacement, one
selected existing file preserved and 41 excluded files. It also exercises
invalid option sequences, empty filters, invalid tape aliases, existing roots,
missing/file parents, leftover staging files, unmatched names and destination
directory/file conflicts. Failures must not produce a successful plan footer;
the real-restore success marker must never appear. The normal unit fixtures
still execute in the same DOS boot.

The cloned test floppy remains writable during the boot. A complete image copy
taken immediately before QEMU must compare byte-identically after every case,
proving no destination root, staging file, directory, file data or metadata was
changed in that run. Test artifacts are in
`build/{US,GR}/dos-tape-tree-preview-large/`. It reads the existing 95-record
OA2101L8 fixture without writing tape. See
[preview completion rules and limitations](restore-preview.md).

English/German `check` builds and DOS preview runs passed on 2026-10-22:
43 files/three directories planned, then one authorized replacement and one
selected file preserved; the complete writable floppy image stayed byte-identical
in both languages. Real selective-restore regressions also passed in both
languages after adding preview. Host ASan/UBSan passed the shared restore and
selection fixtures. No tape data was written; physical qualification remains.

## Staged restore readback

`make dos-readback-smoke` runs DOS backend fault injection in disposable
RBC00–RBC07 directories, followed by a full restore of the existing approved
95-record archive. `RBTEST.EXE` modifies accepted staged bytes, truncates files
and appends trailing bytes, checking that no publication/recovery rename occurs
and that old file bytes and attributes remain intact. Successful new/replacement/
empty cases must publish and leave no staging. Deliberate failure staging remains
in these test directories for inspection. This program is not run in the
byte-identical-image preview mode.

The full archive restore must report 43 staged files read back and checked,
include hidden/system entries in its 43-file tree count, preserve empty files/
directories and leave no restore-root staging/recovery files. Host/DOS source
tests add 20 injectable readback fixtures alongside their existing backup tests;
host ASan/UBSan runs that suite as well. Artifacts are under
`build/{US,GR}/dos-tape-tree-readback-large/`. No tape data writes are required.
See [readback order, failure policy and cache limitations](restore-readback.md).

English/German `check` builds and DOS staged-readback runs passed on 2026-10-22:
eight real-backend cases preserved old destinations on injected corruption/
truncation/trailing data, followed by all 43 archive files successfully reread
before publication. The 20 shared source-verifier fixtures passed on host/DOS
and host ASan/UBSan. Selective-restore and byte-identical-image preview
regressions also passed in both languages. No tape data was written.

## Archive-to-disk comparison

`make dos-compare-smoke` first creates a known restored tree via the read-only-
tape `dos-readback-smoke` setup, then boots a new image containing copies and
negative comparison fixtures. A full 43-file/three-directory comparison and a
two-file filtered comparison must succeed. Equal-length corruption, truncation,
trailing data, missing files/directories, a file/directory conflict and an
unmatched filter name must fail. Replacement/dry-run flags and empty filters
must be refused. Exactly 45 MATCH rows and two success footers are required.

The complete writable comparison image must match its pre-boot copy byte for
byte. This includes the failure fixtures and metadata, not just successfully
checked files. Setup writes only cloned image/host fixtures; the tape is read
only. Artifacts are under `build/{US,GR}/dos-tape-tree-compare-large/`. The
shared source/restore/filter fixtures still run. See
[comparison semantics and limits](archive-compare.md).

English/German `check` builds and DOS comparison runs passed on 2026-10-22:
43-file full comparison plus two filtered matches, equal-length/short/long
mismatches detected, and the complete writable comparison image byte-identical
after every case. The readback setup, selective-restore and byte-identical-image
preview regressions also passed in both languages. No tape data was written.

## Read-only alias enumeration

`make dos-devices-smoke` tests malformed/duplicate options, rejection of the
disposable NRMT3 ordinary-file lookalike, absent custom letter/dollar namespaces,
and successful discovery after the batch temporarily renames that test file
out of the namespace. The lookalike is restored to its original name and checked
byte-identically. Valid aliases, rejected files and missing names have distinct
rows/counters and exit codes. A subsequent full 95-record archive verification
must succeed without an intervening application rewind, proving the queries
did not consume tape data or change its position in this fixture.

Host/DOS probe suites add 14 enumeration fixtures, including legacy fallback,
ABI/status/open/type/close errors, visitor stop, duplicate addresses and invalid
argument refusal before opening. They retain all prior probe/rewind fixtures.
Artifacts are under `build/{US,GR}/dos-tape-tree-devices-large/`; no tape data is
written. See [enumeration scope and exit semantics](device-enumeration.md).

English/German `check` builds and DOS enumeration runs passed on 2026-10-22:
valid/rejected/missing aliases and all four CLI exit classes exercised, followed
by all 95 archive records verified from the unchanged initial tape position.
The 14 shared enumeration fixtures passed on host/DOS and host ASan/UBSan,
alongside the existing probe/rewind regressions. No tape data was written.

## Interactive DOS device/status screen

`make dos-ui-smoke` uses monitor-injected real keyboard events and captures
80x25 VGA text/attribute memory. It checks startup with no automatic query,
enumeration with a rejected lookalike, explicit selection and status failure,
refresh without auto-selection, Enter status, ignored keys, arrow wrap, stale
status clearing, custom alias/dollar preservation, Esc and no-argument startup.
It also verifies actual QUILIB colors: selected `1Eh`, normal data `1Fh`, help
`7Fh`. Checkpoints are in `dos-tape-tree-ui-large/ui-*.{bin,txt}` per language.

After three interactive sessions, the normal CLI must verify the same 95-record
archive from its initial position despite cancelled and refused rewind requests.
A fourth interactive session tests a confirmed rewind after that verification
has consumed the session. The CLI must then verify the same 95 records again.
Enter, Esc and number keys cancel confirmation without leaving the screen or
changing the selected alias. Missing and ordinary-file aliases fail safely;
confirmed attempts invalidate both cached status and enumeration. Both cases
of the localized confirmation key and a fresh status query are exercised.
Confirmed V now additionally verifies the 95-record archive inside the fourth
screen session. A repeated verification without rewind must fail and display
diagnostic-only counters, not replay the archive. Another explicit R restores
the label for final CLI verification. Enter/Esc/number/R cancellation, ordinary
file and missing custom alias refusal, ignored-key result retention and the
first-filemark limit before confirmation are captured as VGA checkpoints.
The entire writable cloned floppy must remain byte-identical. The test uses
the approved OA2101L8 identity guard and writes no tape data. All existing
memory-only fixtures still run in the same boot. See
[screen scope, controls and reuse](dos-interface.md).

English/German `check` builds and interactive DOS runs passed on 2026-10-22:
keyboard navigation, fresh/cached status transitions, localized help and the
three QUILIB colors checked, followed by verification of all 95 archive records.
The entire writable test image remained byte-identical; no tape data was
written. Local DOS build dependencies now include nested lowercase project
headers, so header-only language changes rebuild the affected DOS objects
without modifying the shared Open386MAX build rules.

The rewind extension tests both localized confirmation cases, cancellation,
missing/regular-file refusal, cleared snapshots and subsequent status queries.
VGA capture refuses dumps from previous runs and waits for the final rendered
help row before accepting a checkpoint; a partially painted selected alias is
not sufficient.

English/German `check` builds and the extended interactive tests passed on
2026-10-22. Each DOS run verified all 95 records before and after the confirmed
UI rewind, with the complete writable floppy image unchanged. No tape data was
written; only the identity-guarded virtual scratch cartridge was repositioned.

## Shared verification-session lifecycle

`test_verify_stream` and DOS `STRMTEST.EXE` retain their 13 streaming cases and
add 22 `ot_verify_session` cases. These cover complete verification with and
without a metadata visitor, open/type/raw-mode/status/read errors, fixed-block
refusal before data reads, corruption, early/empty input, stopped visitors,
close failure after otherwise complete verification, and preserving the primary
format/read failure when close also fails. Invalid names, buffer/capacity,
operations and output arguments must issue no open/read/close calls. Successful
opens receive exactly one close attempt, including visitor/format failures.

The same helper now serves CLI `--verify --all`, listing/export and confirmed
UI verification. Its caller-owned buffer keeps allocation failure before open.
CLI warning/header output is checked before opening. Completion output remains
after checked close; path-list publication remains after verified completion.

English/German `check`, interactive verification and large catalogue/export
regressions passed on 2026-10-22. All 22 lifecycle cases passed on host and DOS,
and the host streaming/lifecycle suite passed ASan/UBSan. The UI accepted all
95 records and one closed session, refused a second attempt without rewind,
and retained a byte-identical writable floppy image. CLI catalogue export
retained all 43 file paths and three directories, followed by both 95-record
rewind/verification passes. Only the approved virtual tape was read/repositioned;
no tape data was written.

## Bounded verified DOS contents view

The shared streaming test also exercises `ot_catalog_capture`, indexed lookup
and cleanup: copied directories/metadata, exact 255-character paths, full 32-bit
file sizes, the 256-entry boundary, sticky errors and malformed/null input.
Six session cases cover a complete 30-entry list and provisional entries after
failed close, corruption, early filemark, visitor overflow and fixed-mode
refusal. Failed attempts are discarded even if their entry count looks complete.

The interactive DOS test now uses L to collect the 46-entry virtual archive,
then checks the first, next and last pages, arrow/page/Home/End navigation, shifted
detail paths, actual yellow/normal QUILIB attributes, ignored device/control
keys, B/Esc return and reload. Confirmation cancellation and missing/custom/
ordinary-file aliases remain non-publishing failures. A repeated L without
rewind must fail instead of replaying contents; explicit R restores the tape
for another load and final 95-record CLI verification. No catalogue files are
created, and the entire writable cloned floppy must remain byte-identical.
See [contents model, controls and limits](archive-catalog.md).

Temporary file-mark coverage adds directory refusal, repeated set/clear,
invalid index/value/arguments, independent equal-path session entries, all/none
at 256 entries, failed-catalogue refusal and reset on cleanup. The interactive
suite checks Space toggling, uppercase A/N, all 43 regular files marked without
any directory, cross-page retention, B/Esc discard and an unmarked reload. Marks
are memory-only annotations, not file output or restore/replacement permission.

English/German `check` and complete DOS marking runs passed on 2026-10-22,
including all/none for 43 files, unmarked directories, cross-page retention and
cleared marks after reload. Host/DOS marking fixtures and host ASan/UBSan passed.
The complete writable cloned floppy images remained byte-identical; the final
CLI verified all 95 records after explicit rewind. No tape data was written.

English/German `check` and complete interactive DOS runs passed on 2026-10-22.
All 46 entries were collected only after checked session/close completion;
both catalogue loads and no-rewind refusal passed. The 256-entry/model and six
catalogue lifecycle cases passed on host/DOS and host ASan/UBSan. The complete
writable test images remained byte-identical, and the final CLI verified all
95 archive records after explicit repositioning. No tape data was written.

## Confirmed marked-path export

`make dos-ui-export-smoke` adds an independently formatted B: floppy while
retaining the source/boot A: image's byte-identical comparison. Only the fourth
screen session receives `--selection-out B:\PICKED.LST`; the existing L/R/V,
navigation and marking tests still run. E must refuse empty marks, consume
Enter/Esc cancellation, save precisely the two marked paths in tape order after
localized confirmation, and refuse a second save with DOS error 80. The selected
marks remain visible. Exact CRLF bytes are compared with expected paths;
KEEP.TXT must remain byte-identical and B: may contain only those two files,
without `OT$LIST.TMP`. No tape data is written.

Normal `dos-ui-smoke` instead checks disabled E without a configured target.
Both modes reject missing/duplicate/overlong UI output options before opening
the screen and retain final 95-record CLI verification. Conversion tests on
host/DOS cover 256 owned paths, refusal of nonempty output objects, empty/null
arguments, exact/case-folded/file-directory ambiguity including unmarked
versions, rollback after an earlier copied path and failed-catalogue refusal.
The host streaming/conversion suite also runs under ASan/UBSan. The existing
selection-writer fault fixtures continue to test exact-write/close/publication
failure. See [export controls and boundaries](catalog-selection-export.md).

English/German `check`, export-enabled DOS runs and normal no-target DOS
regressions passed on 2026-10-22. Exact two-path CRLF export, cancellation,
empty/disabled export, unchanged marks and existing-target refusal passed in
both languages. Conversion fixtures passed on host/DOS and host ASan/UBSan.
The entire A: image stayed byte-identical in all four runs; B: contained only
the expected new list and unchanged KEEP.TXT, without staging debris. Final
CLI verification still accepted all 95 records. No tape data was written.


## Native positioning

```sh
make OPENASPI=../OpenAspi native-api-test
bash tests/dos_native_space.sh
MAXLANG=GR OUT="$PWD/build/native-space-gr" bash tests/dos_native_space.sh
```

The disposable-image harness checks signed SPACE CDB encoding, both range
endpoints, zero/EOD, unchanged rejected packets and no commands on invalid CLI
counts. Host/unsupported/deferred/contradictory/mark faults latch uncertainty;
READY retains it and one successful REWIND clears it before another movement.
Exact counters exclude retries or implicit recovery. Original device files
are preserved. The manager is memory-only; no physical tape is used.

## Native LOAD/UNLOAD qualification

```sh
make OPENASPI=../OpenAspi dos-native-media-smoke
MAXLANG=GR OUT="$PWD/build/native-media-gr" bash tests/dos_native_media.sh normal load-unsupported unload-target
```

The disposable-image harness installs the production native provider and uses
OpenASPI's memory-only `mockmedia.inc`. It checks exact sequential LOAD/UNLOAD
CDB fields, one synchronous submission, empty-drive READY/REWIND/READ failures,
invalid packets left unchanged, multiple handles, active writers, abandoned
clean/dirty sessions, retained uncertainty and explicit rewind recovery.
Host, unsupported, deferred, contradictory, target, BUSY and FILEMARK faults
must not produce successful media completion. Exact counters check that no
implicit filemarks/flushes or retries occur. Normal and legacy residual variants
exercise the SDK and CLI; invalid arguments never submit a CDB, and ordinary
files are preserved. Both language builds are supported. No host tape is used.

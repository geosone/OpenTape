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

## Native cartridge removal qualification

```sh
make OPENASPI=../OpenAspi dos-native-lock-smoke
MAXLANG=GR OUT="$PWD/build/native-lock-gr" bash tests/dos_native_lock.sh normal lock-unsupported unlock-target
```

The installed provider/SDK and CLI use a memory-only PREVENT/ALLOW manager.
Normal and legacy residual variants check exact six-byte CDBs, no data buffer,
32-bit capability negotiation, busy ownership, rejected request preservation,
active/abandoned clean and dirty writer sessions, unchanged native state on
failed non-moving commands, prevented UNLOAD and explicit recovery. The `eof`
variant queues a legacy partial-record filemark, performs LOCK/UNLOCK and then
consumes the pending zero-byte EOF without another READ. It prevents removal
controls from silently changing stream state. Host/unsupported/deferred/
contradictory/target/BUSY/FILEMARK errors remain failures; exact counters reject
hidden retries, marks and flushes. Both CLI languages are supported. Old SDK
headers still build, with the new verbs rejected before device access. No host
tape is attached; physical behavior remains unqualified.

## Logical tape position qualification

```sh
make OPENASPI=../OpenAspi dos-native-position-smoke
MAXLANG=GR OUT="$PWD/build/native-position-gr" bash tests/dos_native_position.sh normal unknown legacy eof unsupported
```

Disposable DOS images use the production provider and memory-only
`mockposition.inc` manager. Tests check exact ten-byte READ POSITION CDBs,
full-width big-endian coordinates, BOP/EOP, BPU/LOLU and position overflow,
short/reserved/missing or oversized residual reports, legacy unverified length,
qualified host/unsupported/deferred/contradictory/target/BUSY/FILEMARK errors,
multiple handles and request preservation. Native FAILED/DIRTY|FAILED survives
observation; active writers remain BUSY. Legacy diagnostics and queued EOF
remain unchanged. Exact counters prohibit hidden readiness/movement/retry/
filemark/flush operations. SDK tests cover guarded packets, capability and
ownership checks, close failures and old-header builds. Both language builds
are covered; no host tape is attached.

## Explicit logical LOCATE qualification

```sh
make OPENASPI=../OpenAspi dos-native-locate-smoke
MAXLANG=GR OUT="$PWD/build/native-locate-gr" bash tests/dos_native_locate.sh normal legacy eof unsupported descriptor
```

A memory-only cursor/partition model independently reports each LOCATE target
through READ POSITION. Current-partition commands must not silently select
partition zero; explicit selection includes partitions 0 and 255. Object
addresses cover 0, WORD boundaries, signed LONG boundaries and `4294967295`.
Tests enforce clean-state ownership, exact target echoes, invalid-request
preservation, missing capability, active and abandoned writers, full CLI range
parsing and rejection before I/O. The `eof` fixture proves that successful
movement clears a queued legacy filemark without pretending it is preserved.

Faults include host, unsupported fixed/descriptor sense, deferred, contradictory,
target BUSY, hard target and FILEMARK. A failed movement deliberately changes
the mock cursor; observation may report it but must preserve FAILED. LOCATE,
SPACE and data sessions remain blocked until explicit successful rewind.
Exact counters prohibit retries, hidden position/readiness probes, marks,
flushes and software fallback. Legacy managers can complete LOCATE while their
position readback remains explicitly unverified. Both languages and old SDK
header builds are qualified; physical devices are not attached.

## Wide logical position qualification

```sh
make OPENASPI=../OpenAspi dos-native-wide-smoke
MAXLANG=GR OUT="$PWD/build/native-wide-gr" bash tests/dos_native_wide.sh normal legacy lonu mpu eof descriptor
```

The memory-only cursor model shares low/high position state between LOCATE(10)
and LOCATE(16); independent long READ POSITION reads it back. Tests cover WORD,
DWORD and unsigned 64-bit limits, decimal output, upper-word echo mismatch,
current/explicit partition, full-width reported partition, short-form overflow
without truncation, independent LONU/MPU, obsolete response bytes, short/missing/
oversized residuals and legacy unverified lengths. Rejected operands submit
no command; exact counters detect hidden probes, retry, marks or fallback.

Both six-/ten-/sixteen-byte autosense offsets are checked against fixed and
descriptor errors. Failed wide moves deliberately alter the cursor and retain
FAILED; observation and READY cannot recover native sessions. Active and
abandoned writers, dirty state, old EOF, explicit rewind and packet guards are
covered. Host helpers round-trip decimal values through
`18446744073709551615` and preserve outputs on overflow or short buffers.
Both language builds and old SDK-header builds are qualified. No physical tape
is attached; real devices and Windows bindings remain unqualified.

## Native DOS control screen

```sh
make OPENASPI=../OpenAspi dos-native-controls-smoke
MAXLANG=GR OUT="$PWD/build/native-controls-gr" bash tests/dos_native_controls.sh normal unknown legacy unsupported
NATIVE_UI_SDK="$PWD/deps/OpenASPI" OUT="$PWD/build/native-controls-old" bash tests/dos_native_controls.sh old-sdk
```

The first command uses real QUILIB drawing/input in disposable DOS images.
QEMU monitor events enter/cancel dialogs, correct decimal and partition errors,
exercise full 64-bit targets and operate the memory-only tape model. VGA dumps
check complete localized values, diagnostic/state warnings and inherited
QLIB data/highlight/help attributes. Exact command counters prohibit hidden
position/readiness probes, movement, retries, marks and flushes.

The old-SDK case requires a checkout without the optional wide/removal header
extensions. It proves unavailable UI keys report the requirement without an
open or fallback command. Normal tests additionally reject an ordinary file
masquerading as a tape alias and a missing alias, while preserving file contents.
See [native controls UI](native-controls-ui.md) for behavior and ownership rules.

## Reported partition layout

`bash tests/dos_native_partitions.sh` runs the installed provider, SDK and CLI
against memory-only MODE SENSE page 11h responses. Each disposable DOS image
checks exact command counts, no implicit probes/retries, active writers,
retained FAILED/DIRTY state, legacy diagnostic preservation and queued EOF.
Fixtures include PS, pending counts above reported maximum, absent size
descriptors, reserved/odd/truncated/duplicate/missing/subpage/trailing-page
errors, bad residual provenance and host/target/deferred/unsupported failures.
The CLI rejects extra/duplicate/refresh arguments, rewind aliases and ordinary
files. Both languages use the same behavior. No host tape is attached.

The native controls normal workflow presses **I**, verifies raw metadata and
the pending-layout warning, then continues the existing positioning workflow.
Layout-specific UI fixtures additionally verify that legacy lengths, malformed
pages and host errors display diagnostics without stale metadata. Old SDK
builds reject **I** without commands. The protocol's host validation
and 16-bit ABI checks also cover the 64-byte partition packet.

## Relative SPACE in DOS controls

Run `make OPENASPI=../OpenAspi dos-native-space-ui-smoke` for the **S** chooser,
signed record/filemark editor and EOD confirmation. `MAXLANG=GR` selects German.
Individual cases use `bash tests/dos_native_controls.sh space-normal` and
`space-legacy`, `space-host`, `space-unsupported`, `space-deferred`,
`space-contradict`, `space-mark`. An older-header build can use
`NATIVE_UI_SDK=$PWD/deps/OpenASPI OUT=$PWD/build/space-ui-old` with `space-normal`.

The real QEMU keyboard/VGA tests compare exact no-data six-byte SPACE CDBs
against an independent fixture sequence. They exercise positive and negative
24-bit limits, both units and EOD, empty/sign/overflow/invalid/sticky input,
backspace, canceled editors/confirmation and fixed alias selection. Fault
cases preserve FAILED after position observation, reject movement without a
CDB and recover only after explicit rewind. Ordinary-file and missing-device
rejections submit no commands; requested file contents stay unchanged.
The count and help text inherit existing QUILIB colors. Host tests cover the
shared decimal parser, output preservation and arbitrarily long invalid input.
No host tape is attached.

## Extended mark spacing

`bash tests/dos_native_extended_space.sh` runs all three additional native SPACE
operations with each signed extreme, positive/negative/zero counts, raw invalid
packets, competing handles, active writers and abandoned FAILED/DIRTY sessions.
The CLI uses the same independent CDB sequence after counter reset and rejects
missing, overflowing, duplicate-device and refresh arguments without commands.
Per-operation fault profiles cover host/unsupported/deferred/contradictory,
filemark and descriptor-sense errors, retained state and explicit recovery.
`MAXLANG=GR` repeats the localized contract. The fixture remains memory-only.

`bash tests/dos_native_controls.sh space-ext-normal` drives actual VGA/keyboard
selection of modes 4/5/6. Fault variants are listed in the controls guide.
Old SDK checks reject all new verbs and menu entries with zero submissions;
basic SPACE remains available. No new native mark writer or read-side
setmark-to-EOF mapping is implied.

## Native setmark reads

`bash tests/dos_native_setmark.sh` runs 35 memory-only installed DOS scenarios:
variable/fixed blocks, fixed/descriptor sense, zero/partial data, legacy ASPI
residual support, missing/conflicting length evidence and transport/target
errors. Setmarks remain separate from FILEMARK, repeated accepted raw reads
submit repeated CDBs, and an archive reader fails before accepting bytes.
Exact command counters prohibit hidden reads, retries, marks and writes.
`MAXLANG=GR` selects a German build. Older-header qualification uses
`SETMARK_SDK=$PWD/deps/OpenASPI OUT=$PWD/build/setmark-old` and the `normal`,
`descriptor`, `fixed`, `fixed-descriptor` cases; each rejects the new result
and submits only its first read.

The actual archive CLI harness supports `setmark`, `combined-setmark`,
`fixed-setmark` and `fixed-combined-setmark`. It creates a valid archive in
its memory-only medium, substitutes a setmark at the terminal boundary and
requires verification to fail. Host SDK tests also reject old provider
FILEMARK+00/03 responses; archive tests cover terminal marks with and without
combined final data. No physical tape is attached.

## Owned SETMARK writes

`bash tests/dos_native_setwrite.sh` runs 26 memory-only installed DOS variants:
fixed/variable writer claims, legacy residual reporting, unfinished close,
explicit abort and nine target/transport failure profiles for each mode.
The independent CDB fixture requires `10 02 00 00 01 00`, no data buffer and
32 sense bytes. Exact counters distinguish setmark attempts, ordinary filemarks,
zero-mark flushes and data writes. Raw invalid requests and unowned/competing
handles submit no command. A missing cached provider bit leaves the SDK claim
and diagnostic unchanged. Failed marks cannot replay through any writer method;
cleanup preserves the first error and explicit rewind permits a new session.

`MAXLANG=GR` selects German. Older-header builds use
`SETWRITE_SDK=$PWD/deps/OpenASPI OUT=$PWD/build/setwrite-old` with `normal fixed`;
these retain ordinary writer finish/close with one flush and no setmark.
Host tests separately cover malformed/overrun/mismatched replies and platform
errors, including cleanup precedence. No physical drive is attached.

## TapeAlert

`bash tests/dos_native_alerts.sh` runs the installed provider/SDK/CLI in
memory-only QEMU DOS images. It covers sparse/all/clear/empty flag sets,
1/32/33/64 boundaries, future parameters, DS, malformed headers/ordering/lengths/
values, negotiated residuals and legacy managers, transport/target/descriptor
failures, retained FAILED/DIRTY state and legacy EOF/diagnostic preservation.
The CLI rejects missing/duplicate acknowledgment and extra/refresh arguments.
Exact CDB counters prohibit an implicit header probe, repeated read or status
poll. `MAXLANG=GR` selects German.

`bash tests/dos_native_controls.sh alerts-normal alerts-all alerts-clear
alerts-empty alerts-legacy alerts-malformed alerts-host` exercises the actual
QUILIB view, required confirmation, cancellation, complete 64-code display,
unknown/clear states and failure diagnostics. `MAXLANG=GR` covers translation.
Old SDK controls/CLI checks submit zero commands for A/--native-alerts, while
the established controls and QUILIB colors retain regression coverage.
No physical tape is attached.

## DOS catalogue restore preview

On 2026-10-24 the memory-only archive QEMU fixtures passed in English and German:
`ui`, `ui-legacy`, `fixed-ui`, `ui-preview-keep`, `ui-preview-invalid`,
`ui-preview-disabled`, `preview-service`, `ui-preview-rewind-fault` and
`ui-preview-read-fault`. The real QUILIB screen confirms cancellation/empty marks,
read-only completion, retained marks and provisional counts after failure.
Exact ASPI totals prove invalid destinations/cancellation submit no tape command,
a failed rewind is followed by no read and a read error causes no retry.

The quiet service fixture checks invalid arguments/duplicates, missing destination
parents, a file used as root, stale selection paths and successful new-file
preview. Its entire writable boot image must remain byte-identical. The UI
existing-file fixture checks original bytes, and all missing-root cases check
that preview never creates the target. CLI native archive roundtrips remain a
separate regression. See [preview behavior and API](restore-preview.md).

## DOS catalogue restore into a new root

On 2026-10-24 all 11 `ui-restore*` fixtures plus `restore-service` passed in both
English and German (24 writable-restore scenarios). These cover actual native,
legacy and 512-byte fixed-block output, one selected file from a two-file tree,
empty/disabled/consumed-cancel flows, rejection of existing and file targets,
failed rewind, hard read failure and corruption after a fully checked file was
already published. Successful extraction is checked byte-for-byte. Unmarked tree
files must be absent; repeated W refuses the existing root without new tape I/O.
Cancellation, disabled action, preflight refusal and failed rewind must leave the
entire writable DOS floppy image byte-identical. Exact mock ASPI totals detect
extra movement, reads and retries. The service tests also check invalid/duplicate
filters, malformed/duplicate/missing/overlong startup options, missing parents,
unmatched selected paths with a retained empty root, checked readback counters
and successful publication. These fixtures never access a host tape device.

The complete English preview regression set, German normal/existing-file preview,
quiet preview service and CLI native archive roundtrip/replacement tests also
passed after introducing the shared result and writable batch. English/German
`make check`, the older pinned OpenASPI SDK build and Markdown link audits passed.
See [GUI restore policy and partial-failure handling](catalog-restore.md).

## GUI per-file replacement grants

On 2026-10-24 all ten `ui-replace*` fixtures and `replace-service` passed in both
English and German (22 replacement scenarios). These exercise the real DOS
screen with native, legacy and 512-byte fixed-block transport; KEEP preview
without grants and replacement preview with grants; read-only originals; a
mixed two-file extraction with only one replacement grant; unmarked-file O,
deselection, A/N behavior, cancellation, disabled W and consumed permissions.
A repeated W without renewed grants refuses the existing root without tape I/O.

Confirmed rewind failure, hard read error, late archive corruption after a
published replacement and leftover `OT$OLD.TMP` are also covered. Exact ASPI
counts detect retries or unexpected movement. Original/kept/unrelated files and
published replacements are checked byte-for-byte. The entire writable DOS floppy
must stay byte-identical for cancellation, disabled W, failed rewind, first-read
failure and staging-preflight refusal. A late archive error retains a successful
per-file replacement counter but never reports overall success.

`replace-service` runs STRMTEST's catalogue tests under DOS and verifies API
replacement/filter subset checks, duplicate and empty-list policy, DOS case
folding, planned replacement counts and actual existing/new-root results with
readback counters. Host tests include 256 grants, independent snapshot ownership,
ambiguity rollback and sticky error handling; ASan/UBSan and leak checking pass.
English/German `make check`, the pinned older OpenASPI SDK build, existing
preview/new-root GUI flows and CLI archive roundtrip/replacement regressions pass.
See [O grants and their lifetime](catalog-restore.md#per-file-replacement-with-o).

## DOS catalogue content comparison

On 2026-10-24 all 15 `ui-compare*` fixtures plus `compare-service` passed in both
English and German (32 comparison scenarios). The real QUILIB screen covers
native, legacy and 512-byte fixed-block transport; a filtered two-file tree;
matching content with different attributes; same-length CRC differences; shortened
and missing files; missing/file roots; leftover recovery files; empty selection;
disabled K; consumed confirmation cancellation and result keys; failed rewind,
hard read failure and late archive corruption after a successful file check.
Every scenario requires the entire writable DOS floppy image to remain
byte-identical. O grants remain present throughout, proving comparison never
consumes replacement intent or opens a writable restore batch.

The tree case compares one selected file while an unselected disk file differs;
only the archive data of the exclusion is verified. File mismatches show the
checked path and explicit content/length reason. Late archive failure retains a
provisional checked-file count without a success result. Exact ASPI totals
confirm preflight/cancel paths issue no tape commands, failed rewind is followed
by no read and faults are not automatically retried.

`compare-service` verifies invalid/null/duplicate filters, invalid aliases and
roots before I/O, missing selected archive paths, successful size/CRC/EOF checks
and zero writable/replacement result flags. Malformed, duplicate, missing and
overlong `--compare-to` startup arguments are also rejected before access.
English/German `make check`, the pinned older OpenASPI SDK build, preview/restore/
replacement GUI regressions and CLI archive roundtrip/replacement tests pass.
See [comparison policy and result semantics](archive-compare.md#dos-catalogue-comparison).

## DOS catalogue selection import

On 2026-10-24 all 13 `ui-import*` fixtures and `import-service` passed in both
English and German (28 import scenarios). The real QUILIB screen covers native,
legacy and 512-byte fixed-block catalogues; successful case-insensitive import;
unknown names after an earlier valid name; malformed/NUL, duplicate, empty,
directory, missing and oversized inputs; consumed confirmation cancellation;
disabled I; unchanged inspection cursor and successful clearing of O grants.
All import-only cases require the entire writable DOS floppy image to remain
byte-identical and the exact ASPI total to stay unchanged by import.

`ui-import-restore` imports NOTE from a two-file archive after INPUT had been
marked with replacement intent. The old mark/grant is removed, the new mark has
no replacement permission, and an independently confirmed W restores only NOTE.
Host checks compare its bytes and require INPUT to remain absent from output.

`import-service` runs the catalogue tests under DOS, checks actual regular-file
list loading, all-or-nothing failure, directory/unknown/version ambiguity and
character-device input refusal without a SCSI submission. The core tests cover
all 256 entries, mutable input lifetime, case folding, sticky catalogue errors,
late name-resolution failure and directory/file ambiguity in both entry orders.
Host ASan/UBSan with leak checking and English/German `make check` pass, as do
the pinned older OpenASPI SDK build, preview/restore/replacement/comparison GUI
regressions and CLI single-file/tree/replacement roundtrips. Source recheck and
checked-close behavior reuse the already tested shared selection loader.
See [input format, atomicity and permission rules](catalog-selection-import.md).

## DOS backup dialog and quiet backup service

On 2026-10-24 all 15 `ui-backup*` fixtures and `backup-service` passed in both
English and German (32 backup scenarios). The real QUILIB dialog exercises
single-file/tree sources, native/legacy/fixed-block output, consumed cancellation
and result keys, fresh confirmation for a second attempt, disabled B, missing
sources, a file supplied as a tree and a regular file used as the tape alias.
Short-write and flush/filemark/finish faults retain errors without retries.
The maximum 16 nested levels below the source root (17 directory entries) pass
through the GUI call chain and independent comparison with the existing 9 KB
DOS stack. A seventeenth nested level fails planning before any tape command.

Every successful GUI/API write is independently rewound and compared against
prepared existing disk files with the CLI. The dialog itself reports readback
pending and performs no automatic rewind or comparison. Exact ASPI totals prove
that cancellation/preflight failures write nothing and detect altered write or
finalization sequences. The separate legacy fixture models two close filemarks
and SPACE -1; native explicit finish retains its own single-mark sequence.

All cases require the complete writable DOS floppy image to stay byte-identical:
only the resident virtual tape receives data, and comparison reads existing files.
`backup-service` checks invalid/unauthorized requests, rejection of authorization
values other than 1, non-sticky authorization after success, complete structured
file/byte/error counters and no console output between API markers.
English/German `make check`, the older pinned SDK build, native-controls renderer/
color checks, existing catalogue operations and CLI file/tree/replacement
roundtrips pass. The OpenASPI build/check also passes with the test-only legacy
fixture extension. No physical tape device is accessed.
See [configuration, write consent and completion limits](dos-backup-ui.md).

## QUILIB mouse action integration

On 2026-10-24 `ui-mouse`, `ui-backup-mouse` and `ui-import-mouse` passed in
English and German (six real PS/2 scenarios). These load CuteMouse into disposable
DOS images and send real QEMU movement/button packets. They cover menu/alias
selection, row/checkbox marks, O/A/N and navigation, modal and blank-area clicks,
drag-out cancellation, preview, native SPACE/LOCATE editing/cancellation,
one-batch backup consent and independently verified tape output, then import and
selective restore of an actual disk file. Exact ASPI totals detect unintended
operations; read-only/tape-output flows keep the full DOS disk image identical.
Restore output is compared byte-for-byte and the excluded file must be absent.

The mapper's host/DOS tests and host ASan/UBSan pass, as do English/German
`make check`, the older pinned SDK build, native-controls keyboard/color checks,
existing keyboard catalogue/backup/restore/replace/compare/import flows and CLI
file/tree/replacement roundtrips. No host tape device is accessed.
See [mouse controls, gesture boundaries and test setup](dos-mouse.md).


## Interactive DOS backup settings

On 2026-10-24 the five form scenarios passed in English and German:
`ui-backup-form`, `ui-backup-form-cancel`, `ui-backup-form-invalid`,
`ui-backup-form-limit-mouse` and `ui-backup-form-tree-mouse`. The real QUILIB
FLDEDIT path covers empty defaults, CLI presets, saved/cancelled drafts,
validation, keyboard navigation and real PS/2 mouse focus/mode/save/cancel.
The field-limit scenario rejects an extra overwrite character at EOF in all
three fields, clamps a blank-tail click, checks Delete/Backspace there and
requires an exact resulting volume string. Settings-only and invalid/cancelled
runs issue zero ASPI commands. Successful file/tree backups issue 19/27 commands
including independent CLI rewind/readback comparison. Every form case leaves
its entire DOS disk image byte-identical. No physical tape is accessed.

`FORMTEST.EXE` also runs the portable form transaction tests in DOS; the host
version passes ASan/UBSan. Tests check ownership, borrowed request strings,
40/31-character limits, missing terminators and rejection without changing
committed state. `make check` passes for US/GR and the DOS frontend builds
against the older pinned OpenASPI SDK. See [configuration and test commands](dos-backup-ui.md).


## Interactive DOS catalogue paths

On 2026-10-24 all six path-setting fixtures passed in English and German:
`ui-paths-cancel`, `ui-paths-disable`, `ui-preview-paths`, `ui-compare-paths`,
`ui-restore-paths` and `ui-import-paths-mouse`. They configure destinations and
list filenames through P, then exercise the actual services with no matching
startup options. Saved presets, rejected/cancelled drafts, empty-field disabling,
retained selection/replacement intent and P during a pending confirmation are
checked in the real QUILIB screen. The mouse fixture configures all five fields,
imports a selection and restores only that file; output bytes are compared and
the unmarked file must remain absent. Preview exports an exact CRLF path list.
Comparison and settings-only/cancel/disable scenarios require the complete DOS
disk image to remain unchanged. Exact ASPI totals stay 10/28/37/53 as appropriate;
settings changes themselves issue no commands.

`PATHTEST.EXE` runs the portable bounded transaction tests under DOS; host tests
also pass ASan/UBSan. `make check` passes for US/GR and the DOS executable builds
against the older pinned SDK. Regression fixtures recheck maximum-depth backup,
backup settings (including mouse/overwrite limits), import, replacement restore
and CLI roundtrip after both editors began sharing `fields_dos_ui.c`.
See [path editor and reproduction commands](catalog-paths-ui.md).


## Offline DOS catalogue search

On 2026-10-24 `ui-search` and `ui-search-mouse` passed in English and German.
The real QUILIB dialog covers mixed-case literal queries, forward/backward
navigation, wraparound, directory hits, one-hit cycles, path-offset reset,
missing-hit cursor retention, Apply/Cancel, empty-query clearing and the shared
40-character overwrite-at-EOF guard. Opening search at a pending preview
confirmation only cancels that confirmation; another S opens search. Every
screen retains the marked file and its exact replacement intent.

All four runs require the unchanged pre-search total of 40 ASPI commands and
byte-identical whole DOS floppy images. `FINDTEST.EXE` runs the portable tests
under DOS, including 256 entries, repeated paths, long path tails, OEM bytes,
invalid inputs, inconsistent lists and unchanged result on failure. Host tests
pass ASan/UBSan, `make check` passes for US/GR and the DOS executable builds
against the older pinned SDK. See [search behavior and test commands](catalog-search.md).


## DOS catalogue directory selection

On 2026-10-24 `ui-directory`, `ui-directory-mouse`, `ui-restore-directory`
and `ui-restore-directory-mouse` passed in English and German. Keyboard and
real PS/2 input toggle one file or a complete directory, complete a partially
selected tree without adding replacement grants, clear its existing grants and
report an empty directory. Enter at a pending preview/result only dismisses it;
it cannot also toggle the group. Selection-only runs retain byte-identical DOS
disk images and exactly 43 ASPI commands. Confirmed restoration publishes and
independently reads back both selected files, compares their bytes, leaves no
staging/recovery files and refuses an existing root on a second attempt, for
an exact total of 53 ASPI commands. All media are memory-only fixtures.

`CSELTEST.EXE` covers boundary/session isolation, untouched outside grants,
atomic rejection, empty trees and all 256 entries. Its nodes and snapshot copies
are allocated individually to respect DOS's 64-KiB pointer/block limits.
`FINDTEST.EXE` now uses individual allocations too; both maximum-size tests
were rerun successfully under DOS in US/GR without truncation warnings. Host
models pass ASan/UBSan, `make check` passes in both languages, and the frontend
builds with the older pinned SDK. See [directory selection and reproduction](catalog-directory-selection.md).


## DOS settings profiles

On 2026-10-24 all seven profile scenarios passed in English and German:
`ui-profile`, `ui-profile-mouse`, `ui-profile-missing`, `ui-profile-corrupt`,
`ui-profile-staging`, `ui-profile-empty` and `ui-profile-device`. Settings are
entered through F/P instead of exceeding the DOS command-tail limit. The real
QUILIB controls exercise discarded filenames, cancelled reviews, consumed result
keys, new-file export with staged readback, existing-target refusal and atomic
import after changing both settings groups. Empty import disables backup and
all five path actions; failed import preserves the complete previous settings.

Python independently constructs the expected 337-byte little-endian/CRC encoding;
exports match it exactly in both languages and leave no staging file. Failure
and read-only scenarios retain whole-disk byte identity. The tape-alias case
places a shadow `RMT0.CFG` file on FAT and verifies that the editor refuses it
before any open/close can rewind tape. Every profile case requires zero ASPI
commands, and loading a profile still leaves B awaiting its normal one-shot
write confirmation.

`SETTEST.EXE` and host tests cover single-byte damage at every wire offset,
CRC-valid invalid fields/version/padding, disabled settings, exact field bounds,
reserved profile filenames and unchanged output on error. Host ASan/UBSan,
US/GR `make check`, the older pinned SDK build and backup/restore/mouse/CLI
regressions pass. See [profile operation and wire format](settings-profiles.md).


## Read-only backup preflight

On 2026-10-24 the eight F7 scenarios passed in English and German:
`ui-preflight-file`, `ui-preflight-tree-mouse`, `ui-preflight-missing`,
`ui-preflight-alias`, `ui-preflight-tree-alias`, `ui-preflight-depth`,
`ui-preflight-limit` and `ui-preflight-disabled`. They check exact counts/bytes,
empty-directory inclusion, no partial totals after errors, source-device shadow
files, depth 16 acceptance, depth 17 refusal and result-key consumption. All
sixteen cases retain byte-identical DOS disk images and zero ASPI commands.
A later B still requires its separate write confirmation.

`PSUMTEST.EXE`/host tests exercise list/metadata validation, atomic output,
256 entries and exact payload carries beyond 4 GiB. The shared source backend
now rejects tape stems before opening, including DOS colon/space spellings and
canonicalized dot/parent paths. The quiet backup-service test directly checks
`RMT0.CFG` and `RMT0\.` refusal, then confirms its unchanged 19-command
backup/readback total and lack of persistent authorization. Profiles reuse the
same pure stem detector.

The extra source canonicalizer exposed the previous GUI stack margin at maximum
backup depth. The DOS application now reserves 12,000 bytes; real maximum-depth
backup and preflight pass in both languages with this bound. US/GR `make check`,
host ASan/UBSan, older pinned SDK builds and profile/import/restore/replacement/
CLI regressions qualify the shared backend change. See [F7 behavior and limits](backup-preflight.md).


## Live backup progress

On 2026-10-24 `ui-backup-progress`, `ui-backup-progress-tree-mouse` and
`ui-backup-progress-flush` passed in English and German. Conditional latency in
the memory-only ASPI fixture exposes real VGA states: 32712/35000 file bytes
with zero completed files, full payload before FILE_END acceptance, provisional
finalization and either successful completion or a forced flush failure. Tree
output completes both files; success is independently rewound/compared. Exact
ASPI totals remain 19/27/8 and whole DOS disk images remain byte-identical.
The latency code is absent unless `ARCHIVE_PROGRESS_PAUSE` is defined.

Host/`PRGTEST.EXE` tests check record-commit notifications, no advance on short
writes, rejected reentry/late registration, empty files, 64-bit payload carry,
reinitialization and finalization failure. Both updated DOS model tests and
live file scenarios were rerun in US/GR. The quiet backup-service case observes
exact event counts and rejects notification before/after unauthorized calls;
its 19-command write/readback roundtrip remains unchanged. Host ASan/UBSan,
US/GR `make check` and the older pinned SDK build pass. Regressions include
maximum-depth, tree, legacy/fixed output, short/finalization errors, preflight,
profiles, directory restore and CLI roundtrip.
See [progress counters and observation API](backup-progress.md).


## Read-side restore/preview/comparison progress

On 2026-10-24 `ui-restore-progress`, `ui-restore-progress-mouse`,
`ui-preview-progress`, `ui-compare-progress`, `ui-restore-progress-tree` and
`ui-restore-progress-late` passed in English and German. Test-only read latency
starts after catalogue loading. Real VGA captures show 32712/35000 bytes before
file commit, full payload before successful file completion, excluded-file
verification and pending filemark/tape close. The late-failure fixture publishes
and reads back one file, then rejects a corrupt session boundary without showing
success. Final file bytes match; excluded output remains absent. Read-only
preview/comparison retain whole-disk identity and O intent. ASPI counts stay
37, 53 (tree) and 36 (late failure).

`RESTEST.EXE`/host tests observe successful blocks through existing fault cases,
reject reentry/free/late registration and preserve cumulative checked bytes
across sessions. A packed block with successful data writes but failed commit
produces no progress callback or checked-byte advance; cleanup still aborts its
staging. Quiet DOS service tests reject invalid observed modes/replacement
permissions before I/O/notification and verify exact successful callback counts.
Updated byte counters and file/tree/late-error screens were rerun in US/GR.
Host ASan/UBSan, both `make check` builds and the older pinned SDK build pass;
regressions cover quiet services, replacement, native fixed/legacy reading,
import, maximum-depth backup and CLI roundtrip. No physical tape is used.
See [counter semantics and observation API](restore-progress.md).

## Offline catalogue files

The [catalogue codec and DOS UI](catalog-files.md) are covered by
`test_catalog_file`, DOS `CATTEST.EXE` and:

```sh
make OPENASPI=../OpenAspi MAXLANG=US dos-catalog-files-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-catalog-files-smoke
```

The keyboard and PS/2 roundtrips preserve live marks on export, refuse an
existing destination, load offline with cleared annotations and block C/W/K/O.
Corrupt, missing, staging and tape-alias cases preserve the previous live view
and full floppy image. Every case checks an exact CDB total, and the saved
metadata is decoded independently by `tests/check_catalog_file.py`.

The `ui-catfile-offline` and `ui-catfile-large` variants additionally require
zero CDBs for the entire run. `CATIO.EXE` in the latter verifies production DOS
streaming output/readback and import beyond 64 KiB; Python validates every
exported long path independently.

## Catalogue batch commands

See [CLI syntax, isolation and publication tests](catalog-cli.md#verification).
`test_catalog_cli` injects failures around stdout and live/regular-file service
boundaries, including late verifier failure and the final post-publication
message. The QEMU matrix covers complete production CLI parsing and file I/O:

```sh
make OPENASPI=../OpenAspi MAXLANG=US dos-catalog-cli-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-catalog-cli-smoke
```

Ten cases per language check native, legacy, fixed-block, offline/empty,
malformed options/files, existing target/staging refusal before tape access,
CRC failure without export, over-64-KiB listing and direct offline UI startup.
Offline, invalid-argument and protected-output cases compare whole floppy
images and require zero CDBs; the corrupt-tape case instead checks its exact
12-command failure boundary and absence of any published catalogue.

## Multiple-source backup sets

See [format, limits and qualification](backup-sets.md#verification).
`test_backup_set` checks streaming manifest transactions, and `BSETTEST.EXE`
runs the same parser boundary/failure cases under DOS. The QEMU matrix uses:

```sh
make OPENASPI=../OpenAspi MAXLANG=US dos-backup-set-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-backup-set-smoke
```

Cases cover native/legacy/fixed write and independent tape comparison,
invalid/duplicate/prefix/alias/canonical-alias inputs, combined entry overflow,
Set UI keyboard/mouse selection, combined F7 counts, v2 profile readback/import,
confirmed writes, progress and the 16/17-level tree boundary. Invalid/limit
cases require zero CDBs and unchanged complete floppy images. New public-service
tests also reject unconfirmed Set requests and missing manifests before output.

## Verification and catalogue progress

See [observer ownership, fault coverage and VGA checks](verification-progress.md).
The existing streaming test now observes all 22 session lifecycle cases and
checks read/close/stream reentry rejection. `test_verify_progress` and DOS
`VPRTEST.EXE` cover packed records, multiple sessions, carry and atomic errors.

```sh
make OPENASPI=../OpenAspi MAXLANG=US dos-verification-progress-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-verification-progress-smoke
```

Six cases per language verify V/L progress with keyboard/mouse, native/legacy/
fixed framing, tree payload and a late corrupt session boundary. The entire
floppy image must remain unchanged and exact CDB totals must match the ordinary
read sequences. Read pauses start at rewind 1 in these isolated fixtures;
existing restore-progress fixtures retain their rewind-3 default.

## DOS package installation

```sh
make dos-install-smoke
```

`tests/dos_install.sh` builds `make dist` for English and German, puts the
package on a floppy and installs it silently with its GDI `INSTALL.EXE` into
a fresh SvarDOS hard disk: `INSTALL /CHECK`, `INSTALL /B:C: /S`, the same
installation again over itself, then `C:\OPENTAPE\INSTALL.EXE /U /B:C:`
(confirmed with R and Enter through the QEMU monitor). The installed files
must equal the package, `PathAppend=` must put `C:\OPENTAPE` into the PATH
once, also after the second installation, and `/U` must take it out again,
remove the files and the `GDI.INI` entry, keep the other AUTOEXEC.BAT lines
and leave a consistent FAT. `/B:C:` matters: booted from a floppy, GDI takes
A: as the system drive otherwise.

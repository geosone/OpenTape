# Staged-file readback before publication

Real DOS restore now automatically rereads every staged regular file before
publishing it, including empty files and explicitly authorized replacements.
No extra option or archive-format change is needed. This adds one full disk
read of each file that will actually be restored; skipped/excluded files and
[`--dry-run`](restore-preview.md) do not enter this path.

The checks are deliberately ordered:

1. The archive verifier accepts the complete physical block and validates
   FILE_END size, fragments and whole-file CRC before calling commit.
2. The destination backend has separately accumulated CRC32 over exactly the
   data accepted by successful writes. It sets the staged timestamp and checks
   the write handle's close.
3. It reopens `OT$PART.TMP` read-only using the shared regular-file source
   backend with `SH_DENYWR`, device rejection and metadata snapshot checks.
4. The shared `ot_source_verify` streams the expected number of bytes through
   a bounded 4 KiB buffer, compares CRC32, requires exact EOF, rechecks the open
   source snapshot and checks the read handle's close. Short reads are legal;
   short files, extra trailing bytes, mismatched content, read/status/close
   failures and impossible accepted counts fail without retry.
5. Only after every check succeeds does the backend recheck the destination
   and enter its existing non-replacing publication/replacement exchange.

For a replacement, the old file's attributes and name remain untouched until
readback has passed. A readback failure leaves available staging data for manual
inspection, reports the readback error and does not publish that file. It never
deletes an old destination or temporary file to force progress. Previously
restored files remain subject to the existing partial-result policy: there is
no whole-restore rollback. See [restore and recovery rules](restore.md).

Success reports the number of staged files read back and checked. This counter
excludes skipped/excluded files; preview never reports it. Allocation/open/read/
snapshot/close failures return restore failure just like other destination
errors. The verifier closes a valid already-open source exactly once after any
outcome and preserves an earlier mismatch/change error over a later close error.
Invalid callback/buffer arguments do not transfer source ownership.

## What this proves, and what it does not

The extra gate checks bytes visible through DOS after closing the writer, not
just archive input or a claimed successful write count. It catches, for example,
a staged file that has the right length but incorrect bytes. It does **not**
bypass DOS/controller caches, force a physical surface reread, guarantee
power-loss durability, authenticate data cryptographically or prevent hostile
concurrent filesystem manipulation. CRC32 is a corruption check, not a MAC.
Source sharing/snapshot checks retain their existing DOS/filesystem limitations;
do not modify the destination concurrently.

## Tests

Host and DOS `SRCTEST.EXE` add 20 readback fixtures: short chunks, one-byte
buffers, exact/short/long/empty files, wrong expected CRC, altered bytes of equal
length, early/EOF read errors, impossible counts, snapshot change, close failure
and preservation of earlier failures. The original backup-source fixtures also
run, ensuring the shared source API's existing behavior stays intact.

DOS `RBTEST.EXE` exercises the real destination backend on eight disposable
cloned-floppy targets. It tests new files, replacements and empty files, and
injects equal-length corruption, truncation and trailing bytes behind the
backend's accepted-write CRC. Failure must retain staging, leave the final name
absent or preserve the old file's exact bytes and read-only/hidden/system
attributes, create no recovery rename, and publish nothing. Successful cases
must publish correct bytes and remove their staging/recovery names. Intentional
failure artifacts remain in the test image for inspection.

`make dos-readback-smoke` runs these DOS backend cases, then restores the
existing approved 95-record OA2101L8 archive. All 43 files, including both
nonempty files, must pass staged readback. The extracted tree includes all
hidden/system entries and empty directories. RBTEST is run only in this mode,
not inside the byte-identical-image preview test. No tape overwrite permission
is required and no tape data is written; only disposable image targets change.

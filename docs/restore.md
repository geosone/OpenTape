# Initial DOS restore

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER
```

By default the target directory **must not exist**, even if empty. Its parent must already
exist. This experimental command creates a new local DOS destination, reads
from the current tape position through the first filemark and restores all
supported records in that prefix. Position the tape at its native label first.
It never rewinds, writes tape or controls a changer. Replacement requires an
explicit per-file selection as described below; there is no blanket overwrite.

To extract only specific regular files, add [`--only` or `--only-list`](selective-restore.md).
This restricts extraction without granting replacement permission; excluded
archive data still undergoes all CRC/session checks.

Add [`--dry-run`](restore-preview.md) to verify the same policy and preview
file actions without creating or changing anything in the destination.

## Per-file replacement selection

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --replace ROOT/FILE.BIN
```

One or more `--replace ARCHIVEPATH` options allow an existing destination root
and replace only those exact files, matching ASCII case-insensitively. Repeat
the option for more files (at most 256, subject to DOS command-line length).
Use `--replace-list FILES.LST` for longer selections; lists and individual paths
can be mixed. See [list syntax and validation](restore-selection.md).
Names are archive-relative paths, not full DOS paths. Wildcards, duplicate
selections and directory-wide replacement are rejected. An absent selected file
is restored normally; unrelated files outside the archive remain untouched.

Existing unselected files are skipped without changing their bytes or attributes.
Their archive data still undergoes every CRC/size/session check; skipping cannot
hide corrupt input. Output reports replacements and unchanged files separately.
A selection absent from the archive fails at the filemark, rather than silently
succeeding; other files may already have been restored. Selecting a directory
does not authorize replacing its children. Selection is currently CLI-only,
not yet a graphical per-file dialog.

Archive paths are confined below the destination root. Only the existing relative DOS
8.3 component policy is accepted: no drive prefixes, absolute paths, `..`,
backslashes, wildcards or DOS device names. Duplicate paths ignoring ASCII case,
file/directory prefix collisions and more than 256 metadata entries fail. The
root must resolve to a drive-letter DOS path with valid short components;
UNC paths are rejected. DOS destination paths are limited to 127 bytes including
the root. Use local DOS/FAT storage, not filesystem redirects, links or a
destination concurrently modified by another program. This is not a hardened
sandbox for hostile concurrent filesystem manipulation.

The reserved top-level names `OT$PART.TMP` and `OT$OLD.TMP` (also as parents)
are refused in an archive or selection. Targets already containing either name
are refused before data reads: inspect prior incomplete work first.
At most one regular file is active:

1. The entire physical archive block passes framing, checksums and prospective
   file/session verification **before any output callback for that block**.
2. Metadata creates parents and exclusively creates the staging file below the
   new root. `_dos_creatnew` never truncates an existing file.
3. Data is written once with an exact accepted count. Short/error writes fail;
   they are never retried or silently completed from a remainder.
4. FILE_END must pass size, fragment and whole-file CRC checks. Only then does
   the backend set the file time, check close, and
   [read back staging with exact length/CRC/EOF and checked close](restore-readback.md).
   Only then is staging renamed to its final path using DOS rename without
   implicit replacement. Unexpected destinations
   appearing after a new-file decision fail instead of being overwritten.
5. Supported file attributes (read-only, hidden, system, archive) are restored.
   A zero archived DOS date leaves the newly created timestamp unchanged.

For an explicitly selected existing file, the original remains untouched until
staging, whole-file CRC, time, write close and staged readback have passed. The backend then temporarily
clears read-only if needed and renames the original to `OT$OLD.TMP`. It publishes
the checked new file and applies its attributes. **Only after those steps succeed**
is the selected old version deleted from the recovery name. Successful replacement
does not retain old destination versions permanently. If publication fails, it
attempts a non-replacing rollback and restores original attributes. A competing
destination is never deleted to force rollback. Metadata/cleanup errors retain
available recovery files and fail; rollback errors are reported separately.
This is not a crash-atomic exchange or whole-session transaction.

Empty files and empty directories are supported. Implicit parent directories
are created as needed; directories currently retain normal creation attributes
and timestamps rather than replaying archived directory metadata. Directory
date/attribute restoration remains pending. File metadata errors after rename
still fail the command even if its verified bytes already have their final name.
There is no power-loss or whole-session transaction guarantee.

Success requires at least one closed, internally consistent session, the first
filemark, no active file, and successful tape-reader close. Input uses a fixed
32 KiB buffer with checked archive counters, without the old 16-record test
limit. Unknown record types, fixed-block reassembly, oversized physical records,
VOLUME_END and cross-volume continuation remain unsupported. Distinct paths in
multiple sessions before one filemark can be restored; repeated archive paths
still fail, rather than implicitly authorizing replacement of an earlier version.

Exit `0` means this prefix was accepted under the selected replacement/skip policy;
it does not mean skipped destinations match their archived data. Exit `1` is command syntax,
invalid device selection or selection preflight failure; `2` is a target, format, source, allocation or I/O
failure. On failure, verified files and created directories remain in the
target; `OT$PART.TMP` and/or `OT$OLD.TMP` may remain. They are **not a complete restore**.
There is no whole-operation rollback or automatic deletion of partial results;
per-file publication rollback is limited to restoring the selected original.
Close/free are single-shot,
and a failed or freed core instance cannot continue or retry. Choose a different
new target and explicitly reposition/recover before another attempt.

Use the separate [`--compare`](archive-compare.md) command to check existing
file lengths/CRCs against the archive without restoring or changing anything.
This also provides a read-only check for files intentionally skipped by restore.

## Tests

Host and DOS `RESTEST.EXE` fixtures cover empty/nonempty files, CRC failure
before publication, corrupt data blocks without output callbacks, case collisions,
file-as-parent conflicts, reserved staging names, backend begin/commit/directory
failures, short/impossible/error-with-full-count writes, incomplete sessions,
distinct-path multi-session counters and cleanup without double close or reuse.
The same fixtures pass host AddressSanitizer/UndefinedBehaviorSanitizer.
Skip tests require corrupt skipped files to fail too. Exchange-controller tests
inject failure at every attribute/rename/delete step, including failed rollback
and a competing destination. Each failure must retain original contents at the
destination or recovery name; deletion occurs only after checked publication.

`make dos-restore-smoke` requires the existing boot/device variables and
`MHVTL_ALLOW_OVERWRITE=OA2101L8`, with the same exact virtual-cartridge identity
and WP gates. It defaults to the large tree fixture. Three independent boots
write the native archive, verify it, then run the real restore CLI into RESTORED
inside the cloned DOS floppy image. Only setup writes the disposable virtual
tape; the restore command uses read-only tape access.

The extraction test refuses existing populated and empty roots, restores 43
files and three directories, and compares every file's bytes, size, DOS time/date
and attributes against the source. The fixture includes empty files/directories,
repeated basenames in different folders, and a read-only/hidden/system file.
A repeat call must refuse the existing destination and the entire comparison
must still pass. Host-side checks additionally compare both binary files and
the zero-byte file, confirm the empty directory and require no staging file
after success. Logs and recovered files are in
`build/{US,GR}/dos-tape-tree-restore-large/boot.img` (with sibling serial logs).
Physical tapes, arbitrary archives, disk-full and power-failure recovery remain
unqualified; injected core errors are not a substitute for those hardware tests.

`make dos-replace-smoke` also runs fresh-root restore regression, then explicitly
replaces two seeded files (including a read-only/hidden/system one). A third
unselected file retains its deliberately different data/attributes and an
unrelated destination file stays unchanged. Missing files/empty directories are
restored normally. Byte/metadata comparisons and absence of both temporary names
are required. Logs are under `dos-tape-tree-replace-large/` for each language.

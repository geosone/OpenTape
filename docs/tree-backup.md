# Initial DOS directory-tree backup

```text
OPENTAPE --backup-tree C:\DATA --as DATA --device NRMT0 --overwrite --volume TEST
```

This experimental command writes one selected directory and its descendants as
one new label/session at the **current tape position**. It uses the same explicit
overwrite policy, raw variable-block transport, close/error checks and source
streaming as [single-file backup](single-file-backup.md). It never rewinds,
changes the current DOS directory, controls a changer or appends to an existing
OpenTape volume. Identify and position the correct disposable media first.

Before writable tape open, a read-only preflight builds an in-memory plan:

- The root and every subdirectory become directory metadata, including empty
  directories. `.` and `..` enumeration entries are skipped.
- Regular, read-only, hidden, system and archive files are considered. Volume
  labels are not selected. Each file is opened, its metadata validated and its
  handle closed before preflight continues.
- Relative archive paths follow the existing DOS 8.3 component policy. Names
  that cannot be represented, duplicate archive names ignoring ASCII case,
  allocation failures and enumeration/open/close errors abort the whole operation;
  there is no silent skip or truncation.
- Limits are 256 total entries (root included), 16 descendant-directory levels,
  127-byte DOS source paths and 255-byte archive paths. Wildcards are rejected.
  Exceeding a limit fails before writable tape open. The plan is freed on error.

After preflight, entries stream in DOS enumeration order with increasing object
IDs. Directories are checked for existence/attributes before metadata output.
Files are reopened with deny-write sharing and must still match planned size,
time/date, attributes and archive path. The existing source streamer performs
EOF/growth/recheck/close checks before FILE_END. Any failure poisons the writer
and prevents SESSION_END; tape cleanup cannot turn it into a successful backup.
Root-directory date/time are zero (not available through the attribute query);
descendant-directory timestamps come from the initial DOS enumeration.

This is a frozen selection list, **not an atomic filesystem snapshot**.
Directory membership and timestamps are not continuously rescanned. Files added
after preflight may be absent; same-size changes with unchanged timestamps can
escape detection if sharing is not enforced. Quiesce writers first. There are
no exclusion lists, general selection manifests, multiple unrelated roots, LFN
support, source snapshots or catalogue rebuild yet. Initial
[new-root restore](restore.md) is available separately. A tree can contain
many files; plain `--verify` stops at 16 physical records and returns `3` if no
filemark was confirmed. Use `--verify --all` for a longer session through one
filemark. Do not interpret the short diagnostic's limit as
a completed verification or a write failure. Whole-volume closure is still pending.

`core/backup_plan.c` is independent of DOS. Host and DOS `PLANTEST.EXE` tests
cover multi-entry output, empty directories/files, duplicate-case names, the
256-entry limit, changed metadata, open/directory errors, sink failures and
cleanup without successful finalization after failure. Host tests also pass
AddressSanitizer/UndefinedBehaviorSanitizer.

`make dos-tree-smoke` requires the existing mhVTL variables and explicit
`MHVTL_ALLOW_OVERWRITE=OA2101L8`. The guarded write boot exercises the actual
CLI against a staged DOS tree, including missing-root refusal. A second boot
rebuilds the expected plan and checks every entry, file timestamp/attributes,
data byte and completion boundary. Directory timestamps are not compared across
boots because fresh test-directory creation changes them. A third boot uses
`OPENTAPE --verify --all`. Logs are under
`build/{US,GR}/dos-tape-tree-{write,read,verify}/`.

The fixture contains three files and three directories: one large DOS binary,
a different binary with the same basename in a subdirectory, a zero-byte file
and an empty directory. It occupies fifteen native blocks, deliberately within
the current diagnostic bound. This overwrites the previous OA2101L8 fixture;
old single-file and eleven-record readbacks require rewriting their own fixture.
Physical drives, protected media and end-of-media handling remain unqualified.

Set `OPENTAPE_TREE_LARGE=1` for the extended test fixture: forty additional
empty files produce 46 entries, 43 files and 95 native records. A separate boot
requires plain `--verify` to stop with exit `3`, while the final `--all` boot
must reach the filemark and complete. Large-run directories have the `-large`
suffix, leaving earlier small-fixture logs separate.

# Initial DOS single-file backup

```text
OPENTAPE --backup-file SOURCE.BIN --as FILE.BIN --device NRMT0 --overwrite --volume TEST
```

This writes one explicitly selected regular DOS file as one new native volume
label and one closed session at the **current tape position**. It does not
rewind, append to an existing OpenTape volume, enumerate directories, expand
wildcards or restore files. Position and identify the correct disposable medium
before use. For bounded recursive selection, see [tree backup](tree-backup.md).
`--overwrite` is mandatory and authorizes destruction from that
position onward; no default device or volume name is chosen for this command.
There is no automated media identity or hardware WP query in the DOS backend.
Write-protected media must fail through the driver write/status path.

The first implementation is experimental. It requires raw variable-block
no-rewind devices with extended saved-error status. Source paths are limited
to 127 bytes and archive names to the existing relative DOS 8.3 component
policy. Archive separators are `/`; no drive prefixes or `..` are allowed.
Volume names contain 1..31 ASCII letters, digits, hyphens or underscores.
Explicit option order is currently required as shown above.

The source is opened read-only with DOS deny-write sharing before the tape is
opened writable. Wildcards, character devices, directories, invalid metadata
and unsupported file lengths are rejected. The initial DOS `filelength` path
supports files up to signed-long range (2 GiB minus one), not general 4 GiB files.
Source size, DOS time/date and supported attributes are archived. Label/session
timestamps use the DOS C runtime clock; correct clock/timezone configuration is
the caller's responsibility. Volume UUID generation is not implemented yet.

Data is streamed in chunks of at most 32,712 bytes into physical native records
of at most 32 KiB. Legal short source reads continue; premature EOF, extra data,
read errors, impossible counts, changed size/time/attributes or source-close
failure poison the writer and prevent FILE_END/SESSION_END completion. The
source is closed before FILE_END. Cleanup still closes the tape; filemarks
alone do not make an incomplete native session valid. Driver errors and delayed
finalization errors are reported without retries.

This is not an atomic filesystem snapshot. DOS sharing enforcement depends on
the environment; undetected same-size concurrent changes or misleading metadata
remain possible. Stop applications that write the source before backing it up.
There is no automatic retry or recovery of an incomplete prefix.

Exit `0` means that the source was consumed and checked, and the driver accepted
the session and finalization. It does not establish readback or restore success.
Exit `1` means invalid syntax/options; exit `2` means source, tape, allocation
or archive failure. Reposition explicitly before a readback. The current
`--verify` defaults to a 16-record diagnostic limit, so it may return `3` for
larger otherwise valid single-file backups. Use `--verify --all` to check through
the first filemark without that bound. No automatic verification follows
writing. An experimental [new-root restore](restore.md) is available separately.

`core/source_file.c` is injectable and independent of DOS file APIs. Host and DOS
`SRCTEST.EXE` tests cover short reads, empty files, truncation/growth, read and
close errors, changed-source reports, impossible counts and sink failures.
The source is closed once and no failed transfer can finalize a successful
session, even if a caller ignores its error.

`make dos-file-smoke` requires the same explicit OA2101L8 overwrite authorization
and mhVTL identity/WP gates as `dos-write-smoke`. It copies an actual DOS binary
to SOURCE.BIN preserving its timestamp, refuses missing authorization and a
missing source, then backs up that file with the production CLI. A second boot
compares archived metadata and every data byte against the original DOS file;
a third boot runs the normal `--verify`. The source remains byte-identical.
Logs reside under `build/{US,GR}/dos-tape-file-{write,read,verify}/`.

This replaces the previous native test fixture on OA2101L8. The fixed eleven-
record `TAPETEST` readback requires rewriting its own fixture first. Physical
drive qualification, end-of-media handling, unrestricted multi-root selection,
catalogue rebuild and general recovery remain separate tasks.

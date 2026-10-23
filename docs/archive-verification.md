# Stateful session verification

`OPENTAPE --verify [--all] [--device NRMT0]` uses the raw tape reader and the strict
native framing/payload scanner, then verifies file/session consistency. It
starts at the current position, which must be the volume beginning, reads at
most 16 records by default, and stops at the first filemark. With `--all`, it
reads until that filemark without the diagnostic 16-record bound. It neither rewinds nor writes
or restores files. The legacy backend still rejects fixed-block archives;
`--native --fixed-block N` provides checked fixed-block reassembly for verify
and scan. See [fixed archive framing](native-archive.md#fixed-block-archives).

This verifier implements a sequential profile:

- One active session, and one active regular file, at a time.
- Session IDs strictly increase within the volume. Object IDs strictly increase
  within each session (gaps are allowed). The writer uses this
  profile; arbitrary unique-ID/interleaved layouts are not supported.
- Block session IDs/times must agree with SESSION_START. Empty blocks cannot
  silently change an active session's identity either.
- Directories have metadata only. Regular files require metadata and FILE_END;
  empty files have no DATA fragments and a zero size/CRC/fragment count.
- Nonempty DATA fragments have contiguous offsets and the current file's ID.
  FIRST occurs only on the first fragment, LAST exactly when the declared size
  is reached. Early/missing LAST, excess bytes and data after LAST fail.
- FILE_END must agree with metadata, accumulated bytes, fragment count and the
  IEEE whole-file CRC32. CRC extension across fragments shares the same CRC
  implementation as block/record checksums; padding is not file data.
- SESSION_END must agree with completed-file/directory counts and the 64-bit
  completed-file byte total. An explicitly incomplete result, an open file, or
  EOF before session closure cannot be reported as successful verification.

Unknown types, including the still-unimplemented VOLUME_END and catalogue
checkpoint payloads, fail with an unsupported-record result. `--scan` remains
the separate framing-only tool that can skip unknown types after CRC checks.
The stateful verifier must not silently skip records whose effect on completeness
it cannot establish. All counters are checked before overflow.

`core/archive_verify.c` commits state only after the whole block passes. It
has no extraction callbacks or filesystem side effects. On rejection, even a
valid earlier record in that block cannot publish a partial state transition.
Callers must propagate that rejection; `finish` only describes the previously
accepted prefix and is not permission to ignore an error. There are no automatic
transport retries or resynchronization attempts.

Exit `0` means this prefix contains at least one closed, internally consistent
session and reaches a filemark without an open session/file. Exit `1` means
invalid arguments, `2` transport/format/state failure, and `3` the 16-record
limit was reached before confirming a boundary (bounded mode only). A label-only tape file is
incomplete under `--verify`, even though `--scan` may accept its framing.

`--all` means all records through **one filemark**, not every tape file or a
complete volume. Position explicitly at the native label beforehand. It uses
the same 32 KiB buffer and checked 32-bit block/record counters; fixed-block
reassembly and records larger than that buffer remain unsupported. It does not
allocate memory proportional to archive length, emit per-block progress or
retry reads after an error. On transport, format, incomplete-session or close
failure it returns `2`; success is printed only after the filemark and successful
reader close. There is no fixed elapsed-time bound: physical reads can take time,
and an absent filemark must not be interpreted as successful completion.

`core/verify_stream.c` contains the injectable stream loop. Its optional API
limit returns `OT_VERIFY_STREAM_LIMIT` without issuing another read. Reaching
exactly the data-record count is not enough: one more read must confirm the
boundary. Host and DOS tests cover a 63-record session, early filemarks, corruption
and transport/status errors beyond record 16, exact-limit refusal and fixed-mode
rejection before data access. Tests pass with host ASan/UBSan as well.

`ot_verify_session` adds the shared open/stream/close lifecycle for CLI `--all`,
listing and the [DOS interface's confirmed V operation](dos-interface.md).
The caller supplies the buffer and optional metadata visitor; the helper owns
the handle, never rewinds/retries, closes on every successful open and preserves
the first failure over a later close error. A close failure after otherwise
complete verification is still failure. Returned verifier counts and metadata
remain provisional unless the helper returns zero. Twenty-two host/DOS lifecycle
fixtures cover success with/without a visitor, open/type/mode/status/read/format/
visitor/close failures, incomplete or empty input, bad arguments before I/O,
and preservation of primary transport/format errors during close failure.

This is not a whole-volume or restore qualification. Multi-filemark scanning,
cross-volume continuation, catalogue rebuild and damaged-record recovery remain
pending. Extraction now has a separate [new-root restore policy](restore.md);
verification itself still has no filesystem side effects. The shared writer
core generates session blocks into a sink. A separately guarded DOS/mhVTL
write/readback fixture now exercises a native session and this CLI; a production
complete backup/restore workflow remains pending. Initial
[single-file](single-file-backup.md) and [tree](tree-backup.md) commands are
available. Passing checks cannot
establish that the source selection was complete, that an external
catalogue is correct, or that an actual restore will succeed.

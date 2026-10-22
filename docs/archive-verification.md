# Stateful session verification

`OPENTAPE --verify [--device NRMT0]` uses the raw tape reader and the strict
native framing/payload scanner, then verifies file/session consistency. It
starts at the current position, which must be the volume beginning, reads at
most 16 records, and stops at the first filemark. It neither rewinds nor writes
or restores files. Fixed-block reassembly remains unsupported, as for `--scan`.

This first verifier implements a bounded sequential profile:

- One active session, and one active regular file, at a time.
- Session IDs strictly increase within the volume. Object IDs strictly increase
  within each session (gaps are allowed). The planned writer will use this
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
limit was reached before confirming a boundary. A label-only tape file is
incomplete under `--verify`, even though `--scan` may accept its framing.

This is not a whole-volume or restore qualification. Multi-filemark scanning,
cross-volume continuation, catalogue rebuild, damaged-record recovery, path
collision/destination policy and extraction remain pending. A native writer
and positive native-tape end-to-end test are also still pending. Passing checks
cannot establish that the source selection was complete, that an external
catalogue is correct, or that an actual restore will succeed.

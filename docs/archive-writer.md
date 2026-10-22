# Native archive writer core

`core/archive_writer.c` generates the sequential profile consumed by the
archive verifier: volume label, session start, directory/file metadata, file
data, file end and session end. It is a reusable application-core API, not yet
a DOS tape-writing backend or user-facing backup command.

The caller supplies a separate scratch buffer and an output sink. Each logical
record is currently encoded as one variable-length physical block, with one
record header and both checksum levels. Block numbers, fragment offsets,
FIRST/LAST flags, file CRCs and completion totals are generated from the accepted
state. The existing codecs and prospective verifier are reused rather than
implementing a second independent format/state policy.

Typical sequence:

```text
init -> volume -> session
                   -> directory metadata
                   -> regular metadata -> data chunks -> file end
                   -> empty regular metadata -> file end
                 -> session end -> sink finalization
                 -> optional next increasing session ID
```

There is no implicit traversal, allocation, seek, rewind, confirmation prompt
or retry. The buffer must hold at least a complete label block (144 bytes).
Data chunks are nonempty and cannot exceed the remaining file size or buffer
capacity minus the 56-byte framing overhead. The caller chooses chunking;
Long-path metadata can require 331 bytes including framing; buffers too small
for a particular record are rejected without truncation or output.
the writer never silently splits or replays a physical write. In-place source
data in the scratch buffer is supported via overlap-safe moves. The block
codec supports payloads already in their final position. Label encoding
canonicalizes zero padding after the first name NUL.

Before a sink call, the encoded block must pass framing, payload and state
verification. Invalid caller input causes no output and does not poison a
healthy writer; scratch contents may change. The caller must propagate every
error: a later completion check describes only the accepted prefix, not source
files or operations that were rejected.

A successful callback must report the exact block byte count. Short writes,
impossible counts or errors permanently poison this writer instance. Even an
error reporting a full count is not treated as success. Verified state is
committed only after exact success; media may already have changed on failure.
Later calls cannot replay the block, generate a terminator, or retry finalization.
The last callback count and underlying error remain available for diagnostics.
Callbacks must be synchronous and cannot retain the buffer or reenter the writer;
reentrant API calls are rejected.

SESSION_END is submitted only after all regular files close consistently.
Then `finish_session` must flush/finalize the sink and report delayed failures.
An error here leaves completion failed even if SESSION_END bytes were accepted.
For a future ASPITAPE backend, successful DOS close alone is insufficient:
driver status must also be checked for delayed filemark/finalization errors.
That backend owns physical marks/handle lifetime and must avoid duplicate
termination. No such device sink is connected yet.

The completion query means a closed, sink-acknowledged session prefix. It is
not proof of physical durability or readback. VOLUME_END and catalogue checkpoint
payloads are not emitted; complete-volume closure, source filesystem traversal,
changed-source handling, media write-protect checks and explicit overwrite
authorization still belong to subsequent implementation steps. Never turn a
failed writer into an automatic retry by zeroing its state: media position must
be explicitly recovered first.

Host and DOS tests use a memory sink and a separate readback verifier. They
cover directories, empty files, multi-block file CRCs, multiple sessions,
in-place source data, corruption after output, exact/short/error/invalid-count
callbacks, finalization errors and blocked reentrancy/replays. A host-only test
roundtrips a maximum 65,535-byte physical block with guard bytes and all byte
values in file data. These are memory/codec tests, not tape-write qualification.

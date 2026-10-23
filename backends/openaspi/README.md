# OpenASPI backend

The first [native DOS adapter](../../docs/native-tape-api.md) implements
query, readiness, synchronous rewind, variable-record reading and explicit
writer sessions using OpenASPI category `80h`. The shared consumer validates
versioned packets, retains diagnostics and keeps platform handles inside the
adapter context. English/German status and control commands supplement the
explicit native archive backend selection. A persistent reader retains
actual byte-count validity and stream conditions on one handle and blocks
further reads after failure. The native writer separates flush/filemark/end
from close and preserves failures through abort. Explicit fixed-block archive
attachments now align/reassemble logical OTB1 blocks with checked zero padding;
DOS selects them with `--native --fixed-block N` (1..4096). Windows bindings
and real-device qualification remain open. The QUILIB frontend selects the native path
with `--native --ui`, including snapshots and explicit confirmed rewind.
[Archive commands](../../docs/native-archive.md) select the native bridge with
`--native`; cleanup never introduces implicit filemarks or replays.

The native API is our common operation/capability contract; its scope is not
limited to the historical ASPITAPE ABI. The legacy provider remains a distinct
compatibility path. The following describes its existing OpenASPI diagnostics.

OpenASPI now provides an explicit sixteen-byte status query (`CH=07h`,
`CL=05h`, `oa_aspitape_mtget16`). Use it for extended diagnostics rather than
assuming the size of `CL=02h`: the default is fourteen bytes, but an OpenASPI
installation with `-2` returns the sixteen-byte layout from the original
1992 C headers. EOF/EOT/rewinding bits differ between those layouts.

A historical-driver fallback must select the correct ABI explicitly;
it cannot infer a third-party driver's layout from the shared function number.

When `OA_MT16_STATE_WRITE_ERROR` (`0200h`) is set, diagnostic fields describe
a saved failed/short raw or buffered write or failed written-session finalization,
not the most recent readiness command. Finalization failures cover filemarks,
backspacing and close rewind for both raw and buffered device names.
Display and retain that failure even if `MTNOP` succeeds. The snapshot survives
close and reopen; only a full, successful physical data write clears it.
A failed buffered session requires reopening, while raw clients may explicitly
submit another write in the same session. Buffered acceptance and zero-length
writes are not acknowledgement. Raw short writes fail even with ignore-EOT
selected, preventing DOS from resubmitting the remainder as another record.
Reopening does not repair media position or authorize blind record replay.

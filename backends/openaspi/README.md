# OpenASPI backend

This optional backend adds diagnostics or capabilities exposed explicitly by
OpenASPI.  Ordinary tape data and control operations must continue to use the
portable legacy backend whenever possible.

OpenASPI now provides an explicit sixteen-byte status query (`CH=07h`,
`CL=05h`, `oa_aspitape_mtget16`). Use it for extended diagnostics rather than
assuming the size of `CL=02h`: the default is fourteen bytes, but an OpenASPI
installation with `-2` returns the sixteen-byte layout from the original
1992 C headers. EOF/EOT/rewinding bits differ between those layouts.

This is a backend integration contract, not a completed DOS I/O backend.
An eventual historical-driver fallback must select the correct ABI explicitly;
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

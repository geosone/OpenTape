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

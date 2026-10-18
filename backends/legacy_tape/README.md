# Legacy tape backend

This is the mandatory interoperability backend.  It will probe `RMT0` through
`RMT3` and `NRMT0` through `NRMT3`, enable DOS raw mode, and use generic IOCTL
category `07h`.  It must work with both historical `ASPITAPE.SYS` and
OpenASPI's compatible `ASPITAP.SYS`.

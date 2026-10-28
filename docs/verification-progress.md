# Live verification and catalogue progress

The DOS device screen's confirmed **V** verification and **L** catalogue read
now display progress while the shared reader verifies archive blocks. The view
uses the existing QUILIB colors and mouse/input lifetime. It does not introduce
a new tape command, output file, automatic rewind, retry or write permission.

The selected alias and native/legacy/fixed backend stay visible. Progress shows:

* Completely checked logical OTB1 blocks and records.
* Completed regular files and directory metadata entries, accumulated across
  all sessions before the first filemark.
* Closed session count and cumulative checked FILE_DATA payload bytes.
* Current file bytes/declared size, or 0/0 for directory metadata.
* The current/last full archive path, spread across six bounded rows.

The totals come from the current read, not an old catalogue. A file count rises
only after its verified FILE_END, so the data byte count can reach its declared
size while the completed-file count remains unchanged. Empty files still count
after FILE_END. New session headers retain aggregate counts and the last path;
a new metadata record changes the displayed path and resets its byte counter.
Directory entries are metadata checks, not replayed directory attributes.

Payload excludes archive framing, padding, metadata and disk readback. Bytes
above 4 GiB use whole MiB rounded down; the current file size/count stays exact.
There is no estimated total tape length, percentage, speed or remaining time.
The verifier's aggregate 32-bit record/block bounds also bound file/directory
counts and ensure the displayed whole-MiB total fits the DOS arithmetic.

## Completion boundaries

Each observation follows complete block validation (including all typed records
and CRC/order checks) and successful metadata visitors. An invalid block or a
failed catalogue visitor produces no event for that block. The underlying
verifier can retain diagnostic state for a block whose visitor failed; this is
not published as a progress snapshot or usable catalogue.

A closed session changes the phase to **filemark and tape close still pending**.
Every displayed count remains provisional. There is no observer event for
filemark, finish, failure, close or final success. Only the normal session return
value after checked filemark/finish/close determines the final device result.
A late close failure therefore cannot be turned into success by a complete-looking
progress display. L keeps its metadata provisional and inaccessible until that
return succeeds; on failure the UI frees the complete provisional catalogue.

The busy view has no active mouse actions and performs no input loop, filesystem
or transport call. It redraws padded fields so shorter paths/counts erase older
text. The final result/catalogue replaces it through the ordinary UI flow.
If the display adapter detects an inconsistent observation, it marks progress
unavailable and leaves the actual archive operation/result unchanged.

## APIs and ownership

`ot_verify_stream_observed`, `ot_verify_session_observed` and
`ot_verify_session_opened_observed` add an optional read-only block observer.
Their ordinary wrappers retain existing behavior with observer = NULL. The
selected DOS wrapper `ot_dos_archive_verify_observed` uses the same legacy,
native and fixed-framing open/read/close paths. CLI and headless operations
continue to use the ordinary wrappers.

Observer state and block bytes are borrowed only for the callback. Do not retain
pointers, mutate data, perform filesystem/tape I/O or reenter any session
constructor/operation. Screen rendering is permitted.
Public reader read/close and low-level stream/opened-session reentry explicitly
reject calls while observation is active. These guards do not make arbitrary
C memory mutation or calling a constructor on the live result safe.

`ot_verify_progress_block` consumes already verified observations in sequence.
It validates its bounds, record decoding, counters and overflow before replacing
its owned snapshot. It is not a second archive verifier or a parser for untrusted
unverified blocks. It tracks packed records and carries payload totals across
32-bit boundaries without allocating a complete tape/file image.

## Verification

The streaming-session test injects 22 lifecycle outcomes with an observer:
invalid arguments/open/mode failures, block corruption, read/status errors,
visitor failure, missing boundary, failed close and combined failures. It checks
exact event counts, callback ordering, guarded reader/stream reentry, unchanged
verifier state and no extra reads/closes. `test_verify_progress` and DOS
`VPRTEST.EXE` cover partial/empty files, directory entries, multiple sessions,
packed records, wide byte carry, malformed observations, repeated blocks and
atomic overflow rejection.

The six `ui-verify-progress*` QEMU variants run V and L using keyboard, PS/2
mouse, legacy and fixed transport, a directory tree and a late corrupt catalogue
boundary. Real VGA captures prove partial data, full data before FILE_END,
completed files and the still-provisional filemark phase. The late error drops
the complete-looking catalogue and cannot export it with F8. Complete private
floppy images remain byte-identical. Exact CDB totals are 28 for ordinary,
mouse, legacy and late-error cases, 53 fixed and 40 tree. The fixture alone adds
read latency; production timing and device command sequences are unchanged.
Both build languages are covered. No physical tape or changer is involved.

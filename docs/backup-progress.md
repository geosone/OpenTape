# Live DOS backup progress

After B's normal one-shot confirmation, OpenTape first builds its fresh source
plan. Planning retains its busy panel until a complete source plan is available.
Writing then displays live archive progress in the shared QUILIB screen:

- Current phase: source plan ready, writing archive or finalizing the session.
- Completed archive files and directories against the fresh source plan.
- Accepted file payload against total source payload.
- Current file bytes against its declared length.
- The complete current archive path, across up to six rows.

The selected tape alias and legacy/native/fixed backend remain visible. The
view updates its existing rows rather than clearing the screen for every data
record. Shorter paths erase any remaining characters from the preceding path.
There are no active mouse controls or cancellation keys during synchronous I/O;
this change adds observation only. The result view retains the existing Back
control and consumes its dismissal key/click.

## What the counters mean

A progress update occurs only after a **whole writer record** is accepted by the
sink and its verifier state is committed. A short/failed record does not advance
accepted counters, even if a physical prefix may already have reached the tape.
The final failure result retains the existing uncertain/partial-output diagnostics.

Payload counts are file contents, excluding volume/session/metadata framing,
record overhead, padding and filemarks. Counts use a checked 64-bit high/low
pair. Ordinary sizes are shown in bytes; totals beyond 4 GiB use whole MiB,
rounded down, while the current file's exact 32-bit byte count remains visible.
No estimated speed, time remaining or tape capacity guarantee is implied.

A file is counted complete when its FILE_END record is accepted. In the DOS
source runner this follows exact payload length/EOF, source recheck and checked
source close. Thus all payload bytes can be displayed while completed files
still remain at zero. A directory counts after its metadata record is accepted;
this does not imply directory metadata restore support.

Accepting SESSION_END changes the phase to **finalizing**. It does not establish
successful flush, filemarks, output close or a completed backup. Even a full
payload display and all completed files remain explicitly **provisional** until
the existing runner returns its result after cleanup. A flush/finish failure
still displays the ordinary failure result, never the success message. On
success the result still states that independent **readback is required**.
No implicit rewind/readback, retries, extra tape commands or write grants are
introduced by the progress display.

## Observation API

`ot_archive_writer_observe` attaches a callback before the first record. The
callback sees committed verifier state, the accepted record header and its
borrowed payload. The writer is busy throughout notification: reentry and
late observer replacement are refused. Callback arguments must not be retained
or modified, and callbacks must not perform file/device I/O or control the
transport. Ordinary screen rendering and copying state into caller-owned memory
are supported. A callback has no return value and cannot authorize or cancel I/O.

`core/backup_progress.c` adapts these notifications to source-plan totals, file
counts, payload accounting and a copied archive path. It decodes only small
metadata records; it adds no second payload CRC pass. There is no completion
notification. `ot_dos_backup_execute_observed` shares the original runner and
requires the same exact per-call authorization. Invalid/unauthorized calls do
not notify. The original `ot_dos_backup_execute` and CLI use the same runner
without observers. Only the returned result after cleanup determines success.
`src/backup_dos_ui.c` renders the live snapshot without reentering the writer
or querying the device. Settings profiles and archive wire formats are unchanged.

## Tests

```text
make OPENASPI=../OpenAspi dos-backup-progress-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-backup-progress-smoke
```

Host/`PRGTEST.EXE` tests cover committed-state notifications, writer reentry
refusal, late registration refusal, short writes, empty files, finalization
failure and 64-bit payload carry. The quiet DOS backup-service test checks
initial/intermediate/finalizing observations, exact event counts and zero
callbacks before/after unauthorized calls, then independently compares output.

The three real DOS/QEMU scenarios use conditional latency in the memory-only
ASPI fixture to capture actual VGA intermediate screens. They verify 32712 of
35000 payload bytes with zero completed files, full payload with the file still
open, provisional finalization, successful file/tree completion and a forced
flush failure after all payload bytes were accepted. The tree case uses the
[local CuteMouse fixture](dos-mouse.md). Exact ASPI counts remain 19/27/8;
successful output is independently rewound/compared and every DOS disk image
remains byte-identical. The delay is test-only, preserves its caller state and
is absent unless `ARCHIVE_PROGRESS_PAUSE` is defined. No physical tape is used.

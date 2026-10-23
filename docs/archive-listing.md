# Verified archive contents display

```text
OPENTAPE --list --device NRMT0
```

`--list` shows directory/file entries while reading the native archive from the
current position through its first filemark. It uses the same stateful framing,
payload, whole-file CRC and session-completion checks as `--verify --all`.
There is no 16-record diagnostic limit. Position explicitly at the volume label
beforehand. The default device is NRMT0; only raw no-rewind aliases are accepted.

Output columns are:

```text
Type Bytes Attributes Date Time Session:Object ArchivePath
```

`D` denotes a directory and `F` a regular file, including empty files. Sizes are
decimal bytes, attributes are DOS hexadecimal flags, and dates/times represent
the archived DOS local timestamps. A zero date displays as `0000-00-00`.
Session/object identifiers distinguish entries when a prefix contains more
than one session. Paths are the exact validated short archive paths; duplicate
basenames in different directories are shown separately. Untrusted volume/session
display names are not printed. Archive paths cannot inject control characters
because the shared payload decoder enforces the existing ASCII path policy.

## Completion and limits

The header explicitly labels rows **provisional**. Each metadata row is emitted
only after its complete physical block passes the verifier, but the corresponding
file CRC or session summary may occur later. Bad later input, a missing session
end, read/status/close errors or output errors return exit `2` and report an
incomplete listing. A success footer is printed only after filemark completion,
reader close and checked output flush. Consumers must require exit `0`, not just
the presence of some names or a partial footer on a failing output stream.
Invalid command syntax/device selection returns `1`; `--list --all` is not a
separate mode because listing already reads through the boundary.

This displays one validated tape-file/session prefix, **not a whole-volume
catalogue**, catalogue rebuild database or restore test. It does not rewind,
space over data, write tape or change archived file metadata. Without
`--selection-out`, it does not create disk files either.
The full data stream must be read to verify CRCs, so listing is not an instant
index lookup. Display-only listing uses a fixed 32 KiB buffer and checked archive counters, without
retaining all entries. Mandatory fixed blocks, larger physical records,
VOLUME_END/checkpoints and cross-volume continuation retain their existing limits.

Use the displayed archive paths to review a [replacement list](restore-selection.md).
The formatted contents output itself is **not** a replacement-list file: it also
contains directories, columns and status messages. No overwrite authority is
created by merely displaying a filename, and there is no graphical picker yet.

## Exporting a reviewed replacement list

```text
OPENTAPE --list --device NRMT0 --selection-out FILES.LST
```

This optional export collects regular-file paths only, including empty files,
and writes one exact ASCII archive path per CRLF line. Directories and listing
columns/status messages are not included. The device and output options can be
given in either order. The list is compatible with `--restore --replace-list`,
but **review/edit it first**: retaining a path authorizes replacement of that
existing file when the list is subsequently supplied to restore. Exporting the
list alone neither restores nor grants any active replacement permission.
The same reviewed path format can instead be used with
[`--only-list`](selective-restore.md) to restrict extraction without authorizing
replacement of existing files.

No output file is created until the complete prefix passes CRC/session/filemark
verification, the tape reader closes successfully and listing output flushes.
Export uses the same 256-file limit and path policy as replacement-list input;
duplicate regular-file paths (case-insensitive, including across sessions),
reserved restore paths or allocation failure stop collection and do not publish
a partial list. Ordinary display-only listing does not have this collection
limit. A valid empty prefix produces an empty list.

The destination must be a new local DOS short-name file with an existing parent
directory. Existing files, directories, device names and a pre-existing adjacent
`OT$LIST.TMP` are refused before tape reading. The exporter exclusively creates
that staging name **after verification**, requires exact writes and a successful
close, then publishes using DOS non-replacing rename. An output/close/rename
error returns `2`; a staging file may remain for inspection. Do not use it as a
verified exported list or remove it without inspecting it. A concurrently
created final destination is not overwritten. This is not protection against
hostile directory/device mutation, nor a power-failure durability guarantee.
The command does not rewind after export; explicitly reposition the tape using
[`--rewind`](tape-positioning.md) before restoring a BOT archive. A late console
error can return failure after a valid list has already
been published; inspect reported status and require a successful command.

## Reusable visitor and tests

`ot_verify_stream_visit` adds a synchronous metadata visitor to the existing
stream verifier. The original `ot_verify_stream` calls it without a visitor,
preserving the verification-only path. Callbacks receive a short-lived metadata
pointer plus session/object IDs after whole-block acceptance. A nonzero callback
return stops further input with `OT_VERIFY_VISITOR_FAILED`. A caller must discard
or mark collected entries incomplete on any error; prior rows do not prove later
file/session completion. The visitor is suitable for future selection UI work,
not permission to reuse unverified partial data as a complete catalogue.

Host and DOS `STRMTEST.EXE` cover a complete 63-record listing, exact IDs/names,
visitor failure with immediate read stop, corrupt metadata without a callback
for that block, and a missing session terminator after all rows were seen.
The original transport/limit/error fixtures still run; host ASan/UBSan passes.

`make dos-catalog-smoke` performs a read-only tape test using the existing device
and boot-image variables. It expects the known native tree fixture already on
the approved OA2101L8 virtual cartridge. Set `OPENTAPE_TREE_LARGE=1` for its
95-record/46-entry form; the default expects the 15-record small tree. Recreate
the appropriate fixture with a guarded setup test if another format was loaded.
No overwrite authorization is required for the listing mode itself. The harness
checks scratch identity and explicitly rewinds only that virtual fixture.

DOS argument/refusal tests precede the valid listing. The positive case requires
exactly 43 file rows and three directory rows (large fixture), expected paths,
all 95 verified records and the completed-list footer. US uses explicit device
selection; GR additionally covers the default device. Logs reside under
`build/{US,GR}/dos-tape-tree-catalog-large/`. No physical tapes are qualified.

The same test now exports all 43 regular-file paths, compares their exact set
against the known fixture and rejects device names, an existing destination and
the reserved staging name. An invalid tape alias must leave neither final nor
staging output. A repeated export must refuse the existing list unchanged.
`SELTEST.EXE` and its host equivalent cover 256-path save/load round trips,
empty lists, invalid input and injected write/short-write/over-count/close/publish
failures. No publication callback runs after a write or close failure.

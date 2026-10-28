# Read-only backup source preflight

Configure the backup source with **F**, startup options or an explicitly loaded
profile, then press **F7** on the DOS device screen. The help label can also be
clicked. This inspects the source without opening a tape device or acquiring
output. It reports the complete planned regular-file count, directory count
and payload size. Directory counts include the root and empty directories.
A single-file request reports one file and no directories. Without a configured
source it displays the ordinary F configuration hint and performs no inspection.

Payload size is the sum of file lengths, excluding archive headers, records,
labels, filemarks and other overhead. Up to 4 GiB it is shown in bytes; larger
sums use exact MiB plus remaining bytes. The count uses a portable 64-bit high/
low pair rather than a compiler-specific wide integer. The existing 256-entry,
16-level tree and local DOS path/source limits apply. Counting a tree opens and
closes regular sources for metadata; it does not read their file payloads.

A missing source, invalid file/tree type, device alias, planning limit, allocation,
metadata, recheck or close failure produces an error and **no partial totals**.
Temporary plan entries are freed on every path. The DOS application reserves a 16,000-byte stack for its shared dialogs,
16-level planner and source canonicalization. The action is synchronous; a
busy panel remains visible while enumeration runs and has no active controls.
The result consumes one key/click to return. In particular, pressing B at the
result only dismisses it; another B is needed to open the write confirmation.

## A preview is not a backup grant

F7 does not rewind, probe, read or write tape; choose a tape position explicitly
before writing. It does not reserve media, predict capacity, test source contents,
verify readability of every byte or establish that a backup can finish. Directory
planning is not an atomic filesystem snapshot: files can change and additions
may occur after enumeration. A single-file inspection rechecks its metadata and
checks close, but performs no payload CRC operation.

No plan, source handle or authorization survives the action. **B always builds
its own fresh plan/source snapshot after a separate write confirmation**, then
uses the existing per-file metadata/content/close checks and archive writer.
A successful F7 cannot grant a later batch or bypass source changes. Successful
backup completion still requires the existing explicit tape readback workflow.

## Regular-file device-name boundary

The DOS source backend now rejects tape-device stems before opening any regular
source. This matters even for a failed source check: closing a rewind-on-close
alias could otherwise move media after the handle was identified as a device.
The shared `core/local_name.c` rule covers MT/RMT/NMT/NRMT names, custom letters,
optional `$`, extensions and DOS colon/space delimiters. The source backend also
normalizes dot/parent components for this check and strips a trailing separator
from its checked canonical filename. Profiles reuse the same stem rule.

The backend retains its existing regular-handle and source metadata checks;
filename screening does not replace them. A file present in a FAT directory
under a shadow name like `RMT0.CFG` is still refused, because DOS device-name
lookup can take precedence over that directory entry. Source aliases discovered
inside a tree are refused as well, without exposing partial preflight counts.

## Implementation and tests

`core/plan_summary.c` validates list shape and metadata, then publishes counts
only after the complete supplied list passes. Its caller must first complete
plan construction successfully. `src/backup_preview_dos.c` reuses the existing
DOS source opener/tree planner, checked cleanup and limits. It is quiet and has
no tape alias/backend argument. `src/backup_preview_ui.c` renders the temporary
result through the application's shared QUILIB/mouse controls. Nothing is added
to the settings profile or tape archive format.

```text
make OPENASPI=../OpenAspi dos-backup-preflight-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-backup-preflight-smoke
```

The eight DOS/QEMU cases check a single file, a two-file tree with an empty
subdirectory, missing sources, aliases/shadow files at the root and inside a
tree, maximum accepted depth, excess depth and disabled configuration. Every
case requires zero ASPI commands and a byte-identical whole DOS disk image.
The mouse tree case uses the [local CuteMouse fixture](dos-mouse.md). B after
the result still opens a separate current-position write confirmation.

`PSUMTEST.EXE` and host tests cover empty plans, directories, invalid metadata/
list shapes, unchanged output on failure and all 256 entries at the maximum
32-bit file length, including exact carry above 4 GiB. Nodes are individually
allocated to respect DOS memory segments. The existing quiet backup-service
fixture additionally exercises preview failure/cleanup, direct source alias
refusal (including `RMT0\\.`), and absence of sticky authorization before a
separately confirmed backup and independent CLI comparison. No physical tape
is accessed by these fixtures.

[Backup Set mode](backup-sets.md) reads/rechecks/closes the manifest first, then
plans all file/tree roots under a common archive base. Counts are published only
after complete success, including combined 256-entry and collision checks. The
list contributes metadata/configuration reads; source payloads are still unread.
B rebuilds afresh after its own confirmation.

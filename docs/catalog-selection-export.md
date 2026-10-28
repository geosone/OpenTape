# Export marked DOS catalogue paths

```text
OPENTAPE --ui --device NRMT0 --selection-out C:\LISTS\PICKED.LST
```

Load verified contents with L (explicitly rewind with R first when needed),
mark files using Space/Enter/A/N (or Enter on a directory for its descendant
files), then press E. The question shows the marked-file
count and exact configured output spelling. Only Y in English or J in German
(either case) saves. Every other key cancels and is consumed: Enter never
confirms, Esc does not also leave the view, and selection/navigation keys do
not change marks while the question is open. Cancellation performs no disk or
tape I/O. Export does not read or reposition tape.

The output option is optional and accepts at most 40 characters so the whole
target fits the confirmation screen. Either option order is supported; repeated,
missing or overlong options fail before opening the UI. Relative names resolve
against the current DOS directory. Without an output option or a name saved
with P, E only explains how to configure it. Empty marks
cannot create an empty list.

The output filename can also be configured with **P** in the
[path-settings editor](catalog-paths-ui.md); saving a filename does not create
or overwrite a file. E retains its separate confirmation and new-file policy.

## Exact paths, not replacement permission

The collector's marks refer to individual session/object entries. Export first
copies every marked regular-file path into an owned selection, retaining tape
order and complete paths rather than clipped UI previews. Directories are not
included. A marked path that also occurs in **any other catalogue entry** is
rejected, even if that other entry is unmarked, from another session or differs
only by DOS letter case. A plain path list cannot express which version or a
file/directory collision was intended. No silent deduplication occurs.

Conversion is memory-only and completes before filesystem inspection/creation.
On ambiguity (`rc=-163`), invalid selection or allocation failure, its temporary
copies are freed and nothing is exported. Original marks remain intact on
success, failure and cancellation.

The resulting file is a plain ASCII CRLF path list, compatible with the existing
selection parser. Its intended use is an exact `--only-list` filter:

```text
OPENTAPE --rewind --device NRMT0
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --only-list C:\LISTS\PICKED.LST --dry-run
```

Export does not start restore, grant replacement authority, change CLI defaults
or certify destination safety. Existing-file replacement still requires separate
explicit permission. The format is not role-tagged: deliberately supplying the
file to `--replace-list` is a new replacement authorization and must be reviewed
accordingly. The file carries no media identity or session-version selector.

## Protected publication

The UI reuses `ot_dos_selection_prepare` / `ot_dos_selection_save` and the checked
selection writer already used by CLI listing export. The parent must exist;
devices, existing destinations and existing `OT$LIST.TMP` are refused. The
writer exclusively creates `OT$LIST.TMP` in the target directory, checks exact
writes, closes once and only then publishes by rename. There is no overwrite,
retry, automatic deletion or parent-directory creation.

Success is shown only after publication. Errors retain the list and marks for
inspection; `rc=-130 DOS=80` commonly means the destination or staging file
already exists. After a write/close/rename error, inspect `OT$LIST.TMP` in the
target directory: it may remain for recovery. Do not delete unrelated files.
Esc's normal exit code does not summarize a previously displayed export error.

Replacement intent from O is never encoded in this export. E continues to
export all marked regular-file paths in catalogue order. An exported path list
grants replacement only if the user later explicitly chooses it as a CLI
`--replace-list`; review it for that purpose independently of GUI grant flags.

The [I import action](catalog-selection-import.md) can read this path-list format
back into an already verified catalogue. It atomically replaces extraction marks
and clears O grants only on success; unknown or ambiguous paths leave them intact.

## Tests and limits

Host/DOS conversion tests cover 256 selected paths, independent ownership after
catalogue free, empty/invalid/nonempty-result refusal, exact/case-insensitive
and file/directory ambiguities, rollback after an earlier copied path and failed
catalogue refusal. Existing writer tests cover short/overlong/error writes,
close and rename failure; the host suite also runs with ASan/UBSan.

`make dos-ui-export-smoke` uses the guarded OA2101L8 virtual tape and a separate
disposable B: floppy. It confirms a two-file export, tests Enter/Esc cancellation,
empty marks and repeated-export refusal, and checks exact CRLF bytes plus an
unchanged unrelated file. B: may contain only that unrelated file and PICKED.LST;
no staging file may remain on success or existing-target refusal. The entire
boot/source A: floppy stays byte-identical. Normal `dos-ui-smoke` still tests
disabled export and leaves its whole writable floppy unchanged. Both modes
retain full navigation/marking and final 95-record CLI verification.

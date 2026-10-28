# Backup from the DOS device screen

```text
OPENTAPE --native --ui
OPENTAPE --native --ui --backup-file C:\SOURCE\FILE.BIN --as DATA/FILE.BIN --volume TEST
OPENTAPE --native --ui --backup-tree C:\SOURCE --as DATA --volume TEST
```

On the **device screen**, press **F** to configure the backup, then select the
raw no-rewind alias and press **B** to review and authorize writing. In the
catalogue, B retains its existing meaning of returning to the device screen;
it cannot also start a backup. The dialog names the selected alias, source,
archive-relative path, volume and single-file/recursive-tree mode. It explicitly
warns that writing overwrites from the **current tape position**, without an
implicit rewind. Y confirms in English, J in German. Any other key cancels and
is consumed, including B and Esc.

Use R separately **before** B when writing from BOT is intended. B does not
seek BOT/EOD, choose a cartridge, operate a changer or assert media identity.
Existing tape data at/after the write position may be lost. The existing source
and destination selection in the catalogue does not authorize tape writes.

## Configuration and authorization

`--backup-file SOURCE` and `--backup-tree SOURCE` are mutually exclusive and
must be supplied together with `--as ARCHIVEPATH --volume NAME`. Each may appear
once among `--ui` options. Source and archive path have a 40-character panel
limit, subject to the normal source/DOS archive-path policy. Volume names use
1–31 ASCII letters, digits, hyphens or underscores. Incomplete, duplicate or
invalid configurations are rejected before UI/device access. The existing
native `--fixed-block` prefix may configure the bounded fixed-block backend.
These startup options initialize the session settings; F can edit them later.
Multi-root selection remains pending.

Without startup options, F opens a draft with an empty source, archive path
`DATA`, volume `BACKUP` and file mode. Enter the source, archive-relative path
and volume using the shared **QUILIB FLDEDIT** fields. Tab/Enter/Down moves to
the next field; Shift-Tab/Up moves back. F4 switches file/directory mode. Home,
End, arrow keys, selection, Backspace, Delete and insert/overwrite editing use
the existing QUILIB editor. The shared `src/fields_dos_ui.c` adapter bounds overwrite-at-end and clamps
mouse clicks in a field's blank tail to its actual string end.

**F2 saves** the complete valid draft; **Esc discards** it. Invalid or incomplete
drafts stay in the editor with a validation message, without changing the
previously saved settings. Source wildcards, unsafe/non-DOS archive paths and
invalid volume names follow the same validator as the CLI/service. Saving checks
syntax only: source existence/type and tree planning are checked later by the
confirmed backup runner. Nothing is truncated to make a draft valid.

Settings live only for the current UI session. Reopening F retains saved values;
restarting the program uses its defaults or startup arguments again. F5/F6 on
the device screen can explicitly [export/import these settings](settings-profiles.md). F performs
no file, catalogue or tape I/O and saving never starts a backup or grants write
permission. Until a valid configuration is saved or supplied at startup, B
remains disabled. For catalogue destination/list paths, use the separate [P settings](catalog-paths-ui.md).
Mouse users can select the F label, either mode button, a text
field and the separate Save/Cancel buttons; text entry uses the keyboard. See
[shared mouse operation](dos-mouse.md).

The UI has no sticky `--overwrite` switch: **each B confirmation grants
one synchronous batch only**. A result consumes its dismissal key; B at a result
returns to the device screen and cannot also start a new write. Another B opens
a new confirmation. Disabled/cancelled dialogs perform no source or tape I/O.

After confirmation the shared runner allocates its workspace and opens the
single source or builds the bounded directory plan **before acquiring writable
tape output**. Missing sources, a regular file supplied as a tree and planning
errors stop before any write. The usual 256-entry/16-depth limits, regular-file
checks, metadata rechecks and source-close rules apply. Tree plans are not
filesystem snapshots: per-file checks still run during emission, additions after
planning are not automatically discovered, and a later source error may follow
already written records. See [single-file](single-file-backup.md) and
[directory backup](tree-backup.md) for these shared limits.

## Source preflight

F7 on the device screen can [inspect the configured source](backup-preflight.md)
and report complete file/directory/payload totals without tape access. Errors
expose no partial totals. No plan or grant is retained; B still rebuilds after
its own write confirmation. This does not verify content or media capacity.

## Writing and completion

The GUI calls `ot_dos_backup_execute_observed`, which shares the CLI's source planning,
streaming, archive CRC/session checks, output finalization and cleanup. It emits
the existing native archive format; this is not a new media format. Native,
legacy and explicitly selected native fixed-block transport use their existing
output contracts. In particular legacy no-rewind close retains its historical
two-filemark/backspace policy; native explicit finish retains its own policy.
The UI does not replace either with a private finalization sequence.

Planning retains its busy panel; writing now has a
[live progress view](backup-progress.md) with accepted payload, completed files
and current full archive path. Its counters stay provisional through finalization. Source and tape handles
are closed once; the first operation error survives later cleanup failures.
There are no implicit retries or recovery movements. The CLI warning is checked
and flushed before writable output acquisition; the GUI service is quiet and
returns structured diagnostics rather than printing into QUILIB.

Success means a session was written and the tape handle closed successfully.
The panel reports written file/directory counts and explicitly states that
**readback verification is still required**. Neither archive construction/CRC
checks nor a successful flush/close proves physical media readback. B performs
no automatic rewind, readback or comparison. Position explicitly and use the
existing verify/list/compare/restore tools as appropriate to the archive's start.

On failure the result retains source/DOS, tape/DOS and SCSI/sense/opcode/residual
diagnostics. A source error can show the full active source path across three
rows. Counts are provisional. If output was acquired, the panel warns that tape
contents/position may be uncertain; otherwise it reports that no output was
opened. A partial write is not rolled back or resumed. The driver retains its
own failed/dirty state and recovery requirements. Every confirmed attempt drops
the device screen's cached snapshots; cancellation/disabled B does not.

The later UI DOS exit code does not summarize an error previously displayed in
the dialog. Use the CLI with its explicit `--overwrite` option for scripting.

## API and memory-only tests

`ot_dos_backup_request` contains borrowed immutable source configuration and no
stored write grant. `ot_dos_backup_request_valid` performs syntax checks without
I/O. `ot_dos_backup_execute` requires authorization **exactly equal to 1 on each
call**, rejects invalid raw aliases/configurations first, then returns
`ot_dos_backup_result` after cleanup. `output_opened` records acquisition, not
completion. `finished` requires the complete successful batch and close; file,
directory and high/low payload-byte counters remain provisional on error. The
result retains the final tape status and active source name. No console output
is produced by this service, including failures.

The dialog shares the application's QUILIB line-render callback through
`dos_ui.h`; native controls retain a compatible callback typedef. Windows,
colors, input and screen lifetime remain owned by the main application.

`tests/dos_native_archive.sh` boots disposable DOS images with a resident,
memory-only ASPI medium. The `ui-backup*` cases cover file/tree backup, native,
legacy and fixed blocks; fresh confirmation/cancellation; disabled B; missing,
wrong-type and invalid-device sources/targets; maximum nesting and depth-limit
rejection before writing; short writes and flush/filemark/
finish faults. Each successful GUI backup is independently rewound and compared
against prepared disk files by the CLI. The backup dialog itself still reports
readback pending until that separate operation is performed.

`backup-service` checks invalid/unauthorized requests before any tape command,
non-sticky authorization, structured success counters and absence of console
output between API-call markers. All backup cases require the **entire writable
DOS floppy image to remain byte-identical**, since only the virtual tape is
written and comparison reads existing files. Exact ASPI totals detect unexpected
writes, implicit rewind, retries and changed finalization behavior. No host tape
device or physical cartridge is accessed.


The interactive-form fixtures are available separately:

```text
make OPENASPI=../OpenAspi dos-backup-form-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-backup-form-smoke
```

`test_backup_form`/`FORMTEST.EXE` check owned settings, borrowed requests,
transactional validation, exact 40/31-character bounds and unterminated buffers.
The five QEMU fixtures cover configuration from empty defaults, preset
cancellation, invalid drafts, keyboard navigation, mouse focus/mode/save/cancel,
overwrite-at-EOF rejection and blank-tail editing. Successful file/tree writes
are independently rewound and compared by the CLI. Save-only/cancel/error cases
issue zero ASPI commands, and all five require byte-identical DOS disk images.
The mouse fixtures require the same local CuteMouse archive as `dos-mouse-smoke`.

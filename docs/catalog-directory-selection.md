# Select a DOS catalogue directory tree

In the verified L contents view, move to a directory and press **Enter**, or
click the help panel's **Enter Mark/tree** action. This changes extraction marks
for all regular files below that directory in the same archived session. Nested
subdirectories are included. If every matching file is already selected, Enter
clears their marks; otherwise it selects all of them, including any missing
members of a partially selected tree.

On a regular file Enter toggles that one file, like Space. Space retains its
existing single-file behavior and does nothing on a directory. Directory rows
remain `[-]`; they are not extraction marks themselves. Clicking a directory
row/`[-]` only selects the inspection row. The explicit help action or Enter
then toggles its descendant files. A/N still selects/clears the whole catalogue.

The footer reports selected/total files in the chosen subtree. An empty subtree
reports that it contains no files and changes nothing. Selection is independent
of the visible page and uses full archive paths. Matching ignores ASCII case
and requires a `/` boundary: `DATA` includes `DATA/A.BIN`, but not `DATA2/A.BIN`;
`DATA/SUB` excludes `DATA/SUB2/A.BIN`. A file whose path equals the directory
path is not treated as its descendant. Entries from another archived session
remain unchanged, even if their paths share the same prefix.

## Permissions and restore behavior

Adding marks never grants replacement permission. Existing O grants on files
that were already marked remain intact when a partial subtree is completed.
Clearing a subtree revokes grants only on the files being deselected; marks and
grants outside it are retained. Re-adding a cleared file does not restore a
previous replacement grant. Directories receive no marks or grants.

This operation performs no disk or tape I/O. C/W/K/E continue to copy and validate
the resulting regular-file selection through the existing catalogue services.
Duplicate versions and path ambiguities retain their existing refusal rules;
a group selection does not deduplicate them or authorize overwriting.

To restore, configure a destination with P, select the directory with Enter and
press W. The separate confirmation names the selected files, destination and
replacement-grant count. Existing staging, archive CRC and disk readback checks
still apply to every file. Without exact O grants, an existing restore root is
refused, including one created by an earlier successful restore. Directory
selection at an outstanding operation confirmation only cancels it and is
consumed; another Enter is needed to toggle marks.

This is a convenient regular-file selection, not directory metadata replay.
Empty directories, directory timestamps/attributes and recursive permissions
are not added to the restore selection. Parent directories needed for selected
files are still created by the existing restore backend.

## Implementation and tests

`ot_catalog_mark_tree` in `core/catalog_tree.c` plans a bounded 256-entry bitmap
and validates the complete list, paths, kinds, sessions, flags and annotation
counts before changing anything. Failure leaves all catalogue annotations and
the output result untouched. Valid commits use the existing per-file marking
rules, including replacement-grant revocation. No allocation or I/O is needed
by the production group-selection routine.

```text
make OPENASPI=../OpenAspi dos-directory-selection-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-directory-selection-smoke
```

Host and `CSELTEST.EXE` tests cover partial/all/none transitions, explicit
idempotent add/clear, nested boundaries, ASCII case, session isolation, empty
trees, unrelated grants, malformed snapshots and all 256 entries. DOS tests
allocate individual nodes like the application, avoiding a single array beyond
a 64-KiB segment. Search's maximum-size test uses the same allocation approach.

Real DOS/QEMU keyboard and PS/2 mouse fixtures check file Enter, directory toggle,
partial grant preservation, grant revocation, empty-tree feedback, consumed
confirmation keys and restoration/readback of both selected files. Restored
bytes must match and no staging/recovery files may remain. A repeated restore
must refuse the existing root. Selection-only cases retain byte-identical whole
DOS disks and exactly 43 pre-existing ASPI commands; restoration cases retain
exact totals of 53 commands. Mouse cases use the [local CuteMouse fixture](dos-mouse.md).
No physical tape is accessed.

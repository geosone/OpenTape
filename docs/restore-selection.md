# Replacement lists for DOS restore

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --replace-list FILES.LST
```

Use `--list --selection-out FILES.LST` to [export verified paths](archive-listing.md)
from a positioned native archive. Review and reduce that list before restoring;
every retained filename authorizes replacement. Reposition the tape afterwards;
[`--rewind`](tape-positioning.md) supports archives starting at beginning of tape.

FILES.LST contains exact archive-relative filenames, one per line:

```text
# Only these existing files may be replaced
ROOT/DATA.BIN
ROOT/SUB/DATA.BIN
```

Repeat `--replace-list` or combine it with `--replace PATH`. The combined
selection permits at most 256 unique files. The list avoids DOS's short command
line and replaces the former 16-name selection/bitmask limit. It does not change
the archive's 256-entry extraction limit or DOS destination-path limits.

## Parsing and authority

- Files are ASCII. LF and CRLF line endings are supported, including short reads
  splitting CR/LF. The final line need not end in LF. Bare CR, NUL, Ctrl-Z,
  tabs, non-ASCII bytes and UTF-8 BOMs are errors, not alternate EOF markers.
- Blank lines and full comments starting with `#` in column one are ignored.
  There is no whitespace trimming, quoting, inline-comment or escape syntax.
- Non-comment lines use the same exact relative DOS 8.3 path rules as `--replace`.
  Wildcards, traversal, drive prefixes and reserved staging/recovery names fail.
- A physical line may contain at most 255 printable characters. Each input
  file is limited to 128 KiB, including comments/newlines. Exceeding either the
  combined name count or resource limits aborts; nothing is silently truncated.
- Duplicates ignoring ASCII case fail across all lists and individual options;
  later entries cannot silently broaden or override an earlier decision.

The command checks option syntax first, then opens regular selection files
read-only through the guarded DOS source backend. It reads, validates, checks
for source changes and closes every selection input before opening the archive
reader or changing the destination. All path strings are owned copies; the list
is not consulted again during extraction. A failed load discards every entry
added by that load, and the CLI frees the combined selection and aborts. No
partial list becomes active. Selection-preflight failures return exit `1`.

An empty/comment-only list grants no replacement permission: the default
new-root-only restore policy remains in effect if no other names were selected.
Every selected name must be seen as a file in the archive; an unmatched name
still fails at the boundary. Unselected existing files remain unchanged, and
their archived records are still fully verified. Replacement-list selection is **not**
extraction filtering, wildcard selection, a manifest format change or permission
to overwrite an entire directory. See [restore policy](restore.md) for checked
publication, recovery files and failure semantics.

Extraction filtering uses the separate [`--only`/`--only-list` options](selective-restore.md).
They share this list syntax but do not grant any replacement permission.

The [DOS catalogue O control](catalog-restore.md#per-file-replacement-with-o)
now builds a separate replacement subset of marked files. It uses the same
validation and backend policy, with confirmation and grants consumed per W
batch. It does not load these CLI list files. Do not modify the destination concurrently
with restore or use hostile filesystem redirects.

## Tests

Host and DOS `SELTEST.EXE` fixtures exercise LF/CRLF, EOF without LF, byte-at-a-time
reads, comments/blank lines, cross-input duplicates, invalid/control bytes,
line/file limits, changed input, read/close errors, impossible read counts,
all-or-nothing rollback and 256 unique names (including the last index). The
same tests pass host ASan/UBSan.

`make dos-list-smoke` retains the existing exact OA2101L8 identity, WP and explicit
overwrite gates. Setup writes the approved virtual scratch archive, then normal
new-root restore regression runs in another DOS boot. The final boot rejects a
malformed list and a cross-input duplicate before restoring with a 41-name list
plus one explicit name. All 42 seeded selected files must be replaced, including
empty-file replacements and a read-only/hidden/system file. One unselected file
retains its different contents/attributes, an unrelated file is untouched, and
new directories are created. The existing byte/metadata comparator checks all
selected paths/files and host checks verify preserved files and temporary cleanup.
Logs are under `build/{US,GR}/dos-tape-tree-list-large/`.

The tiny synthetic old files keep the 1.44 MiB DOS test image within capacity;
only these disposable target files and OA2101L8 setup data are overwritten.
The restore command itself still uses read-only tape access. This does not
qualify physical tapes, disk-full rollback, power failures or arbitrary archives.

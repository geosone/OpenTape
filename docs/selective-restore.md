# Exact-file DOS restore filters

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --only ROOT/FILE.BIN
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --only-list FILES.LST
```

`--only PATH` and `--only-list FILE` restrict extraction to exact regular-file
archive paths. They may be repeated and combined. Names are ASCII
case-insensitive and retain their full archive-relative directory prefix.
There is no wildcard, basename matching, directory expansion or subtree
selection. Without either option, full restore retains its previous behavior.

The combined filter must contain 1–256 unique names. Filter files use the
same [checked ASCII list format](restore-selection.md) as replacement lists:
LF/CRLF, optional final newline, blank lines and column-one `#` comments;
no quoting, trimming, controls, BOM or escape syntax. Duplicates within the
filter, including across files and explicit options, fail. Empty/comment-only
combined filters fail rather than being interpreted as “all files”. Inputs
are read-only, checked for changes, closed and copied into owned memory before
destination changes or archive access. Preflight failures return `1`.

## Selection and replacement are separate

A filter does **not** permit overwriting anything. `--replace` and
`--replace-list` remain the only per-file replacement permissions. With a
filter active, every replacement name must also belong to the filter; a
mismatch fails before destination or archive access. The same path may appear
once in each role:

```text
OPENTAPE --restore --device NRMT0 --to C:\RECOVER --only-list PICK.LST --replace-list REPLACE.LST
```

PICK.LST selects what to extract; REPLACE.LST is an optional subset authorizing
which existing selected files may be replaced. Selected existing files without
that permission remain byte/attribute-identical and are counted as skipped.
New selected files are restored normally. The root policy is unchanged:
without any replacement permissions, the destination root must not exist.
A filter does not bypass that restriction.

The [verified archive path export](archive-listing.md) can provide a starting
list for either role. Review/reduce each list for its purpose before use.
Selection never implicitly activates replacement permissions.

Combine the same options with [`--dry-run`](restore-preview.md) to inspect
planned file actions without extracting data or modifying the destination.

## Verification and disk effects

The entire prefix is read through the first filemark, including excluded data.
Framing, payloads, whole-file CRCs and session completion are all verified.
Archive path/collision/reserved-name rules and the 256-metadata-entry extraction
limit also apply to excluded entries. A corrupt excluded file therefore fails
the operation; filtering is not random access or a way around archive errors.

Excluded files do not call the destination's begin/write/commit/abort callbacks.
Explicit directory metadata callbacks are suppressed when filtering; the DOS
backend creates only parents needed for selected files. Unrelated empty
directories are not restored. The directory counter counts explicit archive
directory callbacks, so it is zero in filtered mode even if selected-file
parents were created. File counters distinguish restored files, selected
existing files preserved, and files excluded by the filter.

Every filter name must be seen as a regular file. A missing name or directory-only
name fails with exit `2` at the filemark. Other selected files may already have
been published: there is no whole-operation rollback. Selected files retain
the [existing CRC-gated staging/publication and recovery rules](restore.md).
Failure can leave the destination root and already verified selected files.

Restore starts at the current tape position without implicit rewind. Use
[`--rewind`](tape-positioning.md) explicitly for a BOT archive after listing
or another read. No tape data or changer state is written.

## Tests

Host and DOS restore fixtures cover selected nonempty/empty files, multiple
selected names, case-insensitive matching, absent/directory-only names,
preserved selected files, a later session, missing session end, CRC corruption
in excluded data, excluded duplicate/reserved paths, filter reconfiguration
refusal and index 255 in a 256-name filter. Existing full-restore/exchange
failure tests run unchanged.

`make dos-filter-smoke` uses the existing approved 95-record OA2101L8 virtual
archive without writing tape. Empty/malformed/duplicate filters and replacement
names outside the filter fail before destination creation. A first restore
creates only two selected empty files; a second, after explicit rewind,
replaces one authorized existing file and preserves another selected file
without replacement permission. An excluded nonempty read-only/hidden/system
file and an unrelated file retain bytes and attributes. Unrelated archive
files/directories must not appear. A third pass fails at the boundary for an
unmatched name. Only disposable disk targets in cloned DOS images are modified;
physical devices, concurrent filesystem mutation and power loss are unqualified.

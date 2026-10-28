# Search verified DOS catalogue entries

Load a verified contents view with **L**, then press **S** or click its help
label. Enter up to 40 characters and apply with **F2**; **Esc** discards the
edited draft and retains the previous query. Applying an empty query clears it.
The dialog reuses the same bounded QUILIB field controls as backup/path settings;
text entry is on the keyboard, with mouse focus and Apply/Cancel buttons.

The query is a **literal substring of the complete archive path**, independent
of its clipped row display or horizontal detail offset. ASCII case is ignored;
other DOS/OEM bytes compare exactly. Slash spelling follows archive paths (`/`).
There is no wildcard or regular-expression interpretation. Directories and
separate versions of a repeated path are searchable as individual entries.
Finding an entry does not recursively select it or authorize its extraction.

Applying a query searches forward including the current entry. **F3** finds
the next hit, and **Shift-F3** finds the previous hit, starting beyond the current
entry. Both wrap after one complete catalogue cycle. The help panel's bracketed
F3 and Shift-F3 targets provide the same mouse actions. The footer identifies
the matched catalogue entry index and reports wraparound. With only one match,
a next/previous search returns to that entry with wraparound reported. No match
leaves the inspection cursor and detail offset unchanged. A match resets the
detail offset to zero and the existing page renderer shows its page.

The query is kept only for this running UI session, including later catalogue
loads; no catalogue or settings file is written. A newly loaded catalogue starts
with its ordinary cursor and no stale search-result notice. Ordinary row
navigation clears the search notice while retaining the query for F3. Searching
without a query displays a hint to open S.

## Operation boundaries

Search only inspects the already verified in-memory catalogue. It never opens
files/devices, probes, rewinds or reads tape. Marks, exact per-file replacement
intent, catalogue capture errors and driver status remain unchanged. The
selected inspection row can be a directory; this does not make it markable.

S/F3 at a pending operation confirmation cancels that confirmation and is
consumed; another search command is required to open or advance the search.
At a file-operation result it dismisses that result. Existing one-shot operation
consent and copied selection rules remain in force. Searches do not establish
that the same cartridge is still present; a later tape operation must perform
its existing checks and request its own confirmation.

## Implementation and tests

`core/catalog_search.c` checks query bounds, direction, cursor and linked-list
consistency before publishing a result. It ranks matches by circular distance
in one bounded list pass (up to 256 entries). A no-match, empty query or error
leaves the output result unchanged. `src/search_dos_ui.c` edits a local query
draft using the shared `fields_dos_ui.c` adapter; only a complete valid apply
changes the caller's query. The main UI owns the catalogue, cursor and footer.

```text
make OPENASPI=../OpenAspi dos-search-smoke
make OPENASPI=../OpenAspi MAXLANG=GR dos-search-smoke
```

Host and `FINDTEST.EXE` tests cover all 256 positions, duplicate paths,
directories, long paths beyond the visible row, mixed ASCII case, exact OEM
bytes, literal wildcard text, wraparound, single matches, missing hits, invalid
queries and malformed lists. Query/result failures preserve output and
catalogue marks/replacement intent. Host tests also run under ASan/UBSan.

The real DOS/QEMU keyboard and PS/2 mouse fixtures check search editing,
Apply/Cancel, next/previous navigation, wraparound, path-offset reset, no-match
cursor retention, empty-query clearing, the shared overwrite-at-EOF limit,
marks/replacement preservation and an outstanding operation confirmation.
They require exactly the pre-search 40 ASPI commands and a byte-identical
whole DOS disk image. Mouse tests use the [local CuteMouse fixture](dos-mouse.md).
No physical tape is used.

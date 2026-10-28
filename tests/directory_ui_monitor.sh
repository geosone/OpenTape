# Verified tree catalogue; root + two files (+ EMPTY in selection-only runs).
# Directory selection is offline and all operations retain separate consent.
source "$ROOT/tests/mouse_monitor_helpers.sh"
directory_toggle() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 14; else ui_key ret; fi
}
directory_root() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 20 9; else ui_key home; fi
    sleep 0.15
}
ui_key down; sleep 0.15
# Enter on an ordinary file has the same semantics as Space.
directory_toggle
ui_capture directory-file '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
ui_key o
ui_capture directory-file-grant '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
directory_root
directory_toggle
ui_capture directory-partial '(Directory: 2 of 2 files selected.|Verzeichnis: 2 von 2 Dateien ausgewaehlt.)'
grep -Eq '(Marked: 2 files; replace: 1|Auswahl: 2 Dateien; Ersetzen: 1)' "$OUT/ui-directory-partial.txt"
grep -Eq '\[!\].*INPUT.BIN' "$OUT/ui-directory-partial.txt"
grep -Eq '\[x\].*NOTE.TXT' "$OUT/ui-directory-partial.txt"
grep -Eq '\[-\].*DATA' "$OUT/ui-directory-partial.txt"
# A pending operation consumes Enter as cancellation, without toggling the tree.
ui_key c
if [[ "$variant" = ui-restore-directory* ]]; then
    ui_capture directory-preview-disabled '(No target:|Ziel fehlt:)'
else
    ui_capture directory-preview-prompt '(Inspect 2 selected files|2 Dateien pruefen)'
fi
ui_key ret
ui_capture directory-consumed '(Marked: 2 files; replace: 1|Auswahl: 2 Dateien; Ersetzen: 1)'
directory_toggle
ui_capture directory-cleared '(Directory: 0 of 2 files selected.|Verzeichnis: 0 von 2 Dateien ausgewaehlt.)'
grep -Eq '(Marked: 0 files; replace: 0|Auswahl: 0 Dateien; Ersetzen: 0)' "$OUT/ui-directory-cleared.txt"
directory_toggle
ui_capture directory-added '(Directory: 2 of 2 files selected.|Verzeichnis: 2 von 2 Dateien ausgewaehlt.)'
grep -Eq '(Marked: 2 files; replace: 0|Auswahl: 2 Dateien; Ersetzen: 0)' "$OUT/ui-directory-added.txt"
if [[ "$variant" = ui-restore-directory* ]]; then
    ui_key w
    ui_capture directory-restore '(REALLY restore 2 selected files|2 Dateien WIRKLICH wiederherstellen)'
    grep -Eq '(Replacement grants for this batch: 0|Ersetzungsfreigaben fuer diesen Lauf: 0)' "$OUT/ui-directory-restore.txt"
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
    elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
    ui_capture directory-restored '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.)'
    grep -Eq '(Files=2 Kept=0 Excluded=0|Dateien=2 Behalten=0 Ausgeschlossen=0)' "$OUT/ui-directory-restored.txt"
    grep -Eq '(Readback=2 rc=0 Recovery=0|Ruecklesen=2 rc=0 Recovery=0)' "$OUT/ui-directory-restored.txt"
    ui_key esc
    ui_capture directory-restored-back '(Marked: 2 files; replace: 0|Auswahl: 2 Dateien; Ersetzen: 0)'
    ui_key w
    ui_capture directory-restore-repeat '(REALLY restore 2 selected files|2 Dateien WIRKLICH wiederherstellen)'
    if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
    ui_capture directory-root-refused '(Stopped; no restore output created.|Gestoppt; keine Restore-Ausgabe angelegt.)'
    ui_key esc
    ui_capture directory-no-grants '(Marked: 2 files; replace: 0|Auswahl: 2 Dateien; Ersetzen: 0)'
else
    directory_toggle
    ui_capture directory-clear-again '(Directory: 0 of 2 files selected.|Verzeichnis: 0 von 2 Dateien ausgewaehlt.)'
    ui_key end
    directory_toggle
    ui_capture directory-empty '(No files in this directory tree.|Keine Dateien in diesem Verzeichnisbaum.)'
    grep -Eq '(Marked: 0 files; replace: 0|Auswahl: 0 Dateien; Ersetzen: 0)' "$OUT/ui-directory-empty.txt"
    grep -Fq 'DATA/EMPTY' "$OUT/ui-directory-empty.txt"
fi
ui_key b
ui_capture directory-device '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

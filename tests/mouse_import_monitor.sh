source "$ROOT/tests/mouse_monitor_helpers.sh"
# Entered after the shared keyboard fixture loaded a verified tree catalogue.
ui_mouse_click 4 10
ui_mouse_click 60 16
ui_capture mouse-import-old '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_mouse_click 60 12
ui_capture mouse-import-prompt '(Load file selection from|Dateiauswahl laden aus)'
ui_mouse_click 25 21
ui_capture mouse-import-cancel '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_mouse_click 60 12
ui_capture mouse-import-confirm '(Load file selection from|Dateiauswahl laden aus)'
ui_mouse_click 5 21
ui_capture mouse-imported '(Imported 1 marks; replacement grants cleared.|Auswahl geladen: 1; Ersetzen geloescht.)'
grep -Eq '\[x\].*NOTE.TXT' "$OUT/ui-mouse-imported.txt"
ui_mouse_click 60 20
ui_capture mouse-restore-prompt '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen)'
grep -Eq '(Replacement grants for this batch: 0|Ersetzungsfreigaben fuer diesen Lauf: 0)' "$OUT/ui-mouse-restore-prompt.txt"
ui_mouse_click 5 21
ui_capture mouse-restored '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.)'
grep -Eq '(Files=1 Kept=0 Excluded=1|Dateien=1 Behalten=0 Ausgeschlossen=1)' "$OUT/ui-mouse-restored.txt"
ui_mouse_click 60 5
ui_capture mouse-restored-back '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
ui_mouse_click 60 13
ui_capture mouse-import-device '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 13

source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
ui_capture mouse-start '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 5
ui_capture mouse-scan 'Scan 1 rc=-151'
ui_mouse_click 5 11
ui_capture mouse-unit 'NRMT1'
ui_mouse_click 60 9
ui_capture mouse-missing '(Status failed rc=-201 DOS=2|Status fehlgeschlagen rc=-201 DOS=2)'
ui_mouse_click 5 10
ui_capture mouse-unit-back 'NRMT0'
ui_mouse_click 60 15
ui_capture mouse-rewind-cancel '(Really rewind|wirklich zurueckspulen)'
ui_mouse_click 40 6
ui_capture mouse-blank-keeps-confirm '(Really rewind|wirklich zurueckspulen)'
ui_mouse_click 25 21
ui_capture mouse-rewind-cancelled '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 15
ui_capture mouse-rewind-confirm '(Really rewind|wirklich zurueckspulen)'
ui_mouse_click 5 21
ui_capture mouse-rewound '(Rewound; query status again.|Zurueckgespult; Status neu abfragen.)'
ui_mouse_click 60 19
ui_capture mouse-catalog-confirm '(Load NRMT0 contents|NRMT0 Inhalt)'
ui_mouse_click 5 21
ui_capture mouse-catalog 'DATA/INPUT.BIN'
ui_mouse_click 4 9
ui_capture mouse-mark '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_mouse_click 22 9
ui_capture mouse-row '000: DATA/INPUT.BIN'
ui_mouse_click 60 16
ui_capture mouse-replace '(replace: 1|Ersetzen: 1)'
ui_mouse_click 69 15
ui_capture mouse-none '(Marked: 0 files|Auswahl: 0 Dateien)'
ui_mouse_click 60 15
ui_capture mouse-all '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
ui_mouse_click 69 11
ui_capture mouse-path-right '008:'
ui_mouse_click 60 11
ui_capture mouse-path-left '000: DATA/INPUT.BIN'
ui_mouse_click 70 7
ui_capture mouse-page 'DATA/INPUT.BIN'
ui_mouse_click 68 9
ui_capture mouse-end 'DATA/INPUT.BIN'
ui_mouse_click 60 17
ui_capture mouse-preview-confirm '(Inspect 1 selected files|1 Dateien pruefen)'
ui_mouse_click 5 21
ui_capture mouse-preview '(Preview OK; tape closed.|Vorschau OK; Band geschlossen.)'
ui_mouse_click 60 5
ui_capture mouse-preview-back '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_mouse_click 60 19
ui_capture mouse-export-disabled '(No target: --ui --selection-out|Ziel fehlt: --ui --selection-out)'
ui_mouse_click 60 12
ui_capture mouse-import-disabled '(No input: --ui --selection-in|Datei fehlt: --ui --selection-in)'
ui_mouse_click 60 20
ui_capture mouse-restore-disabled '(No target: --ui --restore-to|Ziel fehlt: --ui --restore-to)'
ui_mouse_click 60 5
ui_capture mouse-restore-back '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_mouse_click 60 10
ui_capture mouse-compare-disabled '(No target: --ui --compare-to|Ziel fehlt: --ui --compare-to)'
ui_mouse_click 60 5
ui_capture mouse-compare-back '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_mouse_click 60 13
ui_capture mouse-device-back '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 20
ui_capture mouse-native '(Native tape controls|Native Bandsteuerung)'
ui_mouse_click 60 8
ui_capture mouse-load-confirm '(Execute this command now|Diesen Befehl jetzt ausfuehren)'
ui_mouse_click 25 20
ui_capture mouse-load-cancel '(Cancelled|Abgebrochen)'
ui_mouse_click 60 10
ui_capture mouse-space '(Relative tape positioning|Relative Bandpositionierung)'
ui_mouse_click 5 13
ui_capture mouse-eod-confirm '(Execute this command now|Diesen Befehl jetzt ausfuehren)'
ui_mouse_click 25 20
ui_capture mouse-eod-cancel '(Cancelled|Abgebrochen)'
ui_mouse_click 60 6
ui_capture mouse-block '(Block|block)'
ui_key 1
ui_mouse_click 5 21
ui_capture mouse-partition '(Partition|partition)'
ui_mouse_click 5 21
ui_capture mouse-locate-confirm '(Execute this command now|Diesen Befehl jetzt ausfuehren)'
ui_mouse_click 25 20
ui_mouse_click 60 16
ui_capture mouse-native-back '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 14
ui_capture mouse-backup-disabled '(No source:|Quelle fehlt:)'
ui_mouse_click 60 5
ui_capture mouse-backup-back '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 13

# Sourced by the smoke harness background monitor writer.
source "$ROOT/tests/ui_monitor_helpers.sh"

ui_capture start '(Selected: NRMT0|Ausgewaehlt: NRMT0)'
ui_key d
ui_capture devices 'Scan 1 rc=-151'
ui_key 1
ui_capture select1 '(Selected: NRMT1|Ausgewaehlt: NRMT1)'
ui_key s
ui_capture missing '(Status failed rc=-2 DOS=2|Status fehlgeschlagen rc=-2 DOS=2)'
ui_key d
ui_capture refresh1 'Scan 2 rc=-151'
ui_key 0
ui_capture select0 '(Selected: NRMT0|Ausgewaehlt: NRMT0)'
ui_key ret
ui_capture status 'state=0002 block=0'
ui_key l
ui_capture catalog-cancel-enter-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
ui_key ret
ui_capture catalog-cancel-enter '(Status path:|Statuspfad:)'
ui_key l
ui_capture catalog-cancel-escape-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
ui_key esc
ui_capture catalog-cancel-escape '(Status path:|Statuspfad:)'
ui_key v
ui_capture verify-cancel-enter-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
ui_key ret
ui_capture verify-cancel-enter '(Status path:|Statuspfad:)'
ui_key v
ui_capture verify-cancel-escape-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
ui_key esc
ui_capture verify-cancel-escape '(Status path:|Statuspfad:)'
ui_key v
ui_capture verify-cancel-select-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
ui_key 1
ui_capture verify-cancel-select '(Status path:|Statuspfad:)'
ui_key v
ui_capture verify-cancel-rewind-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
ui_key r
ui_capture verify-cancel-rewind '(Status path:|Statuspfad:)'
ui_key r
ui_capture cancel-enter-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
ui_key ret
ui_capture cancel-enter '(Status path:|Statuspfad:)'
ui_key r
ui_capture cancel-escape-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
ui_key esc
ui_capture cancel-escape '(Status path:|Statuspfad:)'
ui_key r
ui_capture cancel-select-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
ui_key 1
ui_capture cancel-select '(Status path:|Statuspfad:)'
ui_key x
ui_key up
ui_capture select3 '(Selected: NRMT3|Ausgewaehlt: NRMT3)'
ui_key s
ui_capture rejected '(Status failed rc=-3 DOS=0|Status fehlgeschlagen rc=-3 DOS=0)'
ui_key r
ui_capture rejected-prompt '(Really rewind NRMT3|NRMT3 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture rejected-rewind '(Rewind failed rc=-3 DOS=0|Zurueckspulen fehlgeschlagen rc=-3 DOS=0)'
ui_key v
ui_capture verify-rejected-prompt '(Read NRMT3 from current position|NRMT3 ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture verify-rejected '(Verification FAILED rc=-3 DOS=0|Pruefung FEHLER rc=-3 DOS=0)'
ui_key l
ui_capture catalog-rejected-prompt '(Load NRMT3 contents from current position|NRMT3 Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-rejected '(Verification FAILED rc=-3 DOS=0|Pruefung FEHLER rc=-3 DOS=0)'
ui_key down
ui_capture wrap0 '(Selected: NRMT0|Ausgewaehlt: NRMT0)'
ui_key esc

ui_capture custom '(Selected: NRST0\$|Ausgewaehlt: NRST0\$)'
ui_key 2
ui_capture custom2 '(Selected: NRST2\$|Ausgewaehlt: NRST2\$)'
ui_key d
ui_capture customscan 'Scan 1 rc=1'
ui_key s
ui_capture customstatus '(Status failed rc=-2 DOS=2|Status fehlgeschlagen rc=-2 DOS=2)'
ui_key v
ui_capture verify-custom-prompt '(Read NRST2\$ from current position|NRST2\$ ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture verify-custom '(Verification FAILED rc=-2 DOS=2|Pruefung FEHLER rc=-2 DOS=2)'
ui_key l
ui_capture catalog-custom-prompt '(Load NRST2\$ contents from current position|NRST2\$ Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-custom '(Verification FAILED rc=-2 DOS=2|Pruefung FEHLER rc=-2 DOS=2)'
ui_key esc

ui_capture default '(Selected: NRMT0|Ausgewaehlt: NRMT0)'
ui_key esc

# A complete CLI verification has now consumed the session. Rewind it through
# the real UI and require another complete verification after exiting.
ui_capture rewind-start '(Selected: NRMT2|Ausgewaehlt: NRMT2)'
ui_key r
ui_capture missing-prompt '(Really rewind NRMT2|NRMT2 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture missing-rewind '(Rewind failed rc=-2 DOS=2|Zurueckspulen fehlgeschlagen rc=-2 DOS=2)'
ui_key 0
ui_capture rewind-select '(Selected: NRMT0|Ausgewaehlt: NRMT0)'
ui_key d
ui_capture rewind-scan 'Scan 1 rc=-151'
ui_key r
ui_capture rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key shift-j; else ui_key shift-y; fi
ui_capture rewind-done '(Rewound; query status again|Zurueckgespult; Status neu abfragen)'
ui_key s
ui_capture rewind-status 'state=0002 block=0'
ui_key v
ui_capture verify-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key shift-j; else ui_key shift-y; fi
ui_capture verify-done '(Archive verification OK; tape closed|Archivpruefung OK; Band geschlossen)'
ui_key x
ui_capture verify-ignored '(Archive verification OK; tape closed|Archivpruefung OK; Band geschlossen)'
# No automatic rewind: a second verification must not replay that session.
ui_key v
ui_capture verify-again-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture verify-again '(Verification FAILED rc=|Pruefung FEHLER rc=)'
ui_key r
ui_capture verify-rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture verify-rewind-done '(Rewound; query status again|Zurueckgespult; Status neu abfragen)'
ui_key l
ui_capture catalog-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key shift-j; else ui_key shift-y; fi
ui_capture catalog-first '(Entries: 46; shown: 1-10|Eintraege: 46; Anzeige: 1-10)'
ui_key e
if test "$ui_export" = 1; then
    ui_capture export-empty '(Mark files first; no export|Zuerst Dateien markieren; kein Export)'
else
    ui_capture export-disabled '(No target:|Ziel fehlt:)'
fi
ui_key spc
ui_capture mark-directory '(Marked: 0 files|Auswahl: 0 Dateien)'
ui_key down
ui_key down
ui_key down
ui_capture mark-file 'session=1 object=4 attr=27'
ui_key spc
ui_capture mark-one '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_key spc
ui_capture mark-toggle-off '(Marked: 0 files|Auswahl: 0 Dateien)'
ui_key spc
ui_capture mark-toggle-on '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_key end
ui_capture mark-last 'session=1 object=46 attr=20'
ui_key spc
ui_capture mark-two '(Marked: 2 files|Auswahl: 2 Dateien)'
if test "$ui_export" = 1; then
    ui_key e
    ui_capture export-cancel-enter-prompt '(Export 2 marked files to:|2 markierte Dateien exportieren nach:)'
    ui_key ret
    ui_capture export-cancel-enter '(First filemark; no restore authority|Erste Bandmarke; keine Restore-Freigabe)'
    ui_key e
    ui_capture export-cancel-escape-prompt '(Export 2 marked files to:|2 markierte Dateien exportieren nach:)'
    ui_key esc
    ui_capture export-cancel-escape '(First filemark; no restore authority|Erste Bandmarke; keine Restore-Freigabe)'
    ui_key e
    ui_capture export-prompt '(Export 2 marked files to:|2 markierte Dateien exportieren nach:)'
    if test "$MAXLANG" = GR; then ui_key shift-j; else ui_key shift-y; fi
    ui_capture export-done '(Selection saved; no replacement authority|Auswahl gespeichert; keine Ersetzungsfreigabe)'
    ui_key e
    ui_capture export-existing-prompt '(Export 2 marked files to:|2 markierte Dateien exportieren nach:)'
    if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
    ui_capture export-existing '(Export FAILED rc=-130 DOS=80|Export FEHLER rc=-130 DOS=80)'
fi
ui_key home
ui_capture mark-preserved 'session=1 object=1 attr=10'
ui_key shift-a
ui_capture mark-all '(Marked: 43 files|Auswahl: 43 Dateien)'
ui_key shift-n
ui_capture mark-none '(Marked: 0 files|Auswahl: 0 Dateien)'
ui_key down
ui_key down
ui_key down
ui_capture mark-retain-file 'session=1 object=4 attr=27'
ui_key spc
ui_capture mark-retain '(Marked: 1 files|Auswahl: 1 Dateien)'
ui_key home
ui_capture mark-retain-home 'session=1 object=1 attr=10'
ui_key up
ui_capture catalog-top 'session=1 object=1 attr=10'
ui_key down
ui_capture catalog-down 'session=1 object=2 attr=10'
ui_key pgdn
ui_capture catalog-next '(Entries: 46; shown: 11-20|Eintraege: 46; Anzeige: 11-20)'
ui_key pgup
ui_capture catalog-previous 'session=1 object=2 attr=10'
ui_key end
ui_capture catalog-last '(Entries: 46; shown: 41-46|Eintraege: 46; Anzeige: 41-46)'
ui_key down
ui_capture catalog-bottom 'session=1 object=46 attr=20'
ui_key right
ui_capture catalog-right '008: A.BIN'
ui_key left
ui_capture catalog-left '000: ROOT/DATA.BIN'
ui_key home
ui_capture catalog-home 'session=1 object=1 attr=10'
ui_key r
ui_key 3
ui_key ret
ui_capture catalog-ignored '(Entries: 46; shown: 1-10|Eintraege: 46; Anzeige: 1-10)'
ui_key b
ui_capture catalog-back '(Status path:|Statuspfad:)'
ui_key l
ui_capture catalog-again-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-again '(Verification FAILED rc=|Pruefung FEHLER rc=)'
ui_key r
ui_capture catalog-rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-rewind-done '(Rewound; query status again|Zurueckgespult; Status neu abfragen)'
ui_key l
ui_capture catalog-reload-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-reload '(Entries: 46; shown: 1-10|Eintraege: 46; Anzeige: 1-10)'
ui_key a
ui_capture mark-before-escape '(Marked: 43 files|Auswahl: 43 Dateien)'
ui_key esc
ui_capture catalog-escape '(Status path:|Statuspfad:)'
ui_key r
ui_capture catalog-final-rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture catalog-final-rewind-done '(Rewound; query status again|Zurueckgespult; Status neu abfragen)'
ui_key esc

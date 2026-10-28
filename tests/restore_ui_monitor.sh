# Sourced after a fully verified catalogue is visible; memory-only tape fixture.
# No output/export file is created by the monitor itself.
ui_key w
if test "$variant" = ui-restore-disabled; then
    ui_capture restore-disabled '(No target: --ui --restore-to|Ziel fehlt: --ui --restore-to)'
    ui_key esc
else
    ui_capture restore-empty '(Mark files first.|Zuerst Dateien markieren.)'
    ui_key esc
    ui_capture restore-empty-back 'DATA/INPUT.BIN'
    if test "$variant" = ui-restore-tree; then ui_key end; fi
    ui_key spc
    ui_key w
    ui_capture restore-confirm '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen)'
    grep -Eq '(Writes files; NEW destination directory only.|Schreibt Dateien; nur NEUER Zielordner.)' "$OUT/ui-restore-confirm.txt"
    # W is consumed as cancellation, not interpreted as another write request.
    ui_key w
    ui_capture restore-cancelled '(Marked: 1 files|Auswahl: 1 Dateien)'
    if test "$variant" != ui-restore-cancel; then
        ui_key w
        ui_capture restore-confirm-again '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        case "$variant" in
            ui-restore-existing|ui-restore-invalid|ui-restore-rewind-fault)
                ui_capture restore-stopped '(Stopped; no restore output created.|Gestoppt; keine Restore-Ausgabe angelegt.)'
                grep -Eq '(Restore FAILED|Restore FEHLER)' "$OUT/ui-restore-stopped.txt"
                ;;
            ui-restore-read-fault|ui-restore-late-fault)
                ui_capture restore-partial '(Incomplete; files/staging may remain.|Unvollstaendig; Dateien/TMP koennen bleiben.)'
                grep -Eq '(Restore FAILED|Restore FEHLER)' "$OUT/ui-restore-partial.txt"
                if test "$variant" = ui-restore-late-fault; then
                    grep -Eq '(Readback=1 rc=0 Recovery=0|Ruecklesen=1 rc=0 Recovery=0)' "$OUT/ui-restore-partial.txt"
                fi
                ;;
            *)
                ui_capture restore-ok '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.)'
                grep -Eq '(Readback=1 rc=0 Recovery=0|Ruecklesen=1 rc=0 Recovery=0)' "$OUT/ui-restore-ok.txt"
                if test "$variant" = ui-restore-tree; then
                    grep -Eq '(Files=1 Kept=0 Excluded=1|Dateien=1 Behalten=0 Ausgeschlossen=1)' "$OUT/ui-restore-ok.txt"
                fi
                # Existing root is refused even after this UI created it. No
                # preview success or retained mark authorizes an overwrite.
                ui_key esc
                ui_capture restore-again-back '(Marked: 1 files|Auswahl: 1 Dateien)'
                ui_key w
                ui_capture restore-again-confirm '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen)'
                if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
                ui_capture restore-again-refused '(Stopped; no restore output created.|Gestoppt; keine Restore-Ausgabe angelegt.)'
                ;;
        esac
        ui_key esc
        ui_capture restore-retained '(Marked: 1 files|Auswahl: 1 Dateien)'
    fi
fi
ui_key b
ui_capture restore-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

# Selection import operates on the verified offline catalogue, not tape.
ui_key home
ui_key down
ui_key spc
ui_key o
ui_capture import-before '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_key i
if test "$variant" = ui-import-disabled; then
    ui_capture import-disabled '(No input: --ui --selection-in|Datei fehlt: --ui --selection-in)'
else
    ui_capture import-confirm '(Load file selection from:|Dateiauswahl laden aus:)'
    grep -Eq '(Replaces marks; clears replacement grants.|Ersetzt Auswahl; loescht Ersetzungsfreigaben.)' "$OUT/ui-import-confirm.txt"
    # O is consumed as cancellation and cannot revoke the old live grant.
    ui_key o
    ui_capture import-cancelled '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    if test "$variant" != ui-import-cancel; then
        ui_key i
        ui_capture import-confirm-again '(Load file selection from:|Dateiauswahl laden aus:)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        case "$variant" in
            ui-import|ui-import-legacy|ui-import-fixed|ui-import-restore)
                ui_capture import-ok '(Imported 1 marks; replacement grants cleared.|Auswahl geladen: 1; Ersetzen geloescht.)'
                grep -Eq '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)' "$OUT/ui-import-ok.txt"
                grep -Eq '\[x\].*NOTE.TXT' "$OUT/ui-import-ok.txt"
                if cut -c1-53 "$OUT/ui-import-ok.txt" | grep -Fq '[!]'; then
                    echo 'Import retained a replacement grant' >&2; printf 'quit\n'; exit 1
                fi
                # Navigation/inspection cursor stays on INPUT; the imported
                # mark identifies NOTE independently of the inspection row.
                grep -Fq '000: DATA/INPUT.BIN' "$OUT/ui-import-ok.txt"
                if test "$variant" = ui-import-restore; then
                    ui_key w
                    ui_capture import-restore-confirm '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen)'
                    grep -Eq '(Replacement grants for this batch: 0|Ersetzungsfreigaben fuer diesen Lauf: 0)' "$OUT/ui-import-restore-confirm.txt"
                    if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
                    ui_capture import-restored '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.)'
                    grep -Eq '(Files=1 Kept=0 Excluded=1|Dateien=1 Behalten=0 Ausgeschlossen=1)' "$OUT/ui-import-restored.txt"
                    ui_key esc
                    ui_capture import-restored-back '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
                fi ;;
            *)
                ui_capture import-failed '(Import FAILED|Import FEHLER)'
                grep -Eq '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)' "$OUT/ui-import-failed.txt"
                grep -Eq '\[!\].*INPUT.BIN' "$OUT/ui-import-failed.txt"
                if test "$variant" = ui-import-unknown; then
                    grep -Eq '(List path not in catalogue; marks kept.|Listenpfad fehlt im Katalog; Auswahl bleibt.)' "$OUT/ui-import-failed.txt"
                fi ;;
        esac
    fi
fi
ui_key b
ui_capture import-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

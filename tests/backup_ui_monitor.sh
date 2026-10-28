source "$ROOT/tests/ui_monitor_helpers.sh"
ui_capture backup-start '(Devices and tape status|Geraete und Bandstatus)'
ui_key b
if test "$variant" = ui-backup-disabled; then
    ui_capture backup-disabled '(No source:|Quelle fehlt:)'
    ui_key esc
else
    ui_capture backup-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
    grep -Eq '(No rewind; existing tape data may be lost.|Kein Zurueckspulen; alte Daten gehen verloren.)' "$OUT/ui-backup-confirm.txt"
    grep -Fq 'DATA' "$OUT/ui-backup-confirm.txt"
    grep -Fq 'TEST' "$OUT/ui-backup-confirm.txt"
    # B is consumed as cancellation, not another invocation.
    ui_key b
    ui_capture backup-cancelled '(Devices and tape status|Geraete und Bandstatus)'
    if test "$variant" != ui-backup-cancel; then
        ui_key b
        ui_capture backup-confirm-again '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        case "$variant" in
            ui-backup|ui-backup-tree|ui-backup-depth|ui-backup-fixed|ui-backup-legacy)
                ui_capture backup-written '(Session written; tape closed.|Sitzung geschrieben; Band geschlossen.)'
                grep -Eq '(Readback verification still required.|Ruecklesepruefung weiterhin erforderlich.)' "$OUT/ui-backup-written.txt"
                if test "$variant" = ui-backup-tree; then
                    grep -Eq '(Files=2 Directories=1|Dateien=2 Verzeichnisse=1)' "$OUT/ui-backup-written.txt"
                elif test "$variant" = ui-backup-depth; then
                    grep -Eq '(Files=1 Directories=17|Dateien=1 Verzeichnisse=17)' "$OUT/ui-backup-written.txt"
                else grep -Eq '(Files=1 Directories=0|Dateien=1 Verzeichnisse=0)' "$OUT/ui-backup-written.txt"; fi ;;
            ui-backup-missing|ui-backup-bad-tree|ui-backup-device|ui-backup-depth-limit)
                ui_capture backup-stopped '(No tape output opened.|Keine Bandausgabe geoeffnet.)'
                grep -Eq '(Backup FAILED|Backup FEHLER)' "$OUT/ui-backup-stopped.txt" ;;
            *)
                ui_capture backup-failed '(Tape content/position uncertain; inspect.|Bandinhalt/Position unklar; bitte pruefen.)'
                grep -Eq '(Backup FAILED|Backup FEHLER)' "$OUT/ui-backup-failed.txt" ;;
        esac
        # A result B only returns. Another B needs a new explicit confirmation.
        ui_key b
        ui_capture backup-result-back '(Devices and tape status|Geraete und Bandstatus)'
        ui_key b
        ui_capture backup-new-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
        ui_key esc
    fi
fi
ui_capture backup-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

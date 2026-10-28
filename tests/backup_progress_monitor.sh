# Test-only virtual media latency exposes actual intermediate VGA snapshots.
source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
ui_capture progress-start '(Devices and tape status|Geraete und Bandstatus)'
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 14; else ui_key b; fi
ui_capture progress-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture progress-partial '(File bytes: 32712/35000|Dateibytes: 32712/35000)'
grep -Eq '(Payload accepted: 32712/|Nutzdaten: 32712/)' "$OUT/ui-progress-partial.txt"
grep -Eq '(Files closed: 0/|Dateien abgeschlossen: 0/)' "$OUT/ui-progress-partial.txt"
grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-progress-partial.txt"
ui_capture progress-payload-full '(File bytes: 35000/35000|Dateibytes: 35000/35000)'
grep -Eq '(Files closed: 0/|Dateien abgeschlossen: 0/)' "$OUT/ui-progress-payload-full.txt"
ui_capture progress-finalizing '(Finalizing session; please wait.|Sitzung wird abgeschlossen; bitte warten.)'
grep -Eq '(Provisional; finalization/readback pending.|Vorlaeufig; Abschluss/Ruecklesen stehen aus.)' "$OUT/ui-progress-finalizing.txt"
if test "$variant" = ui-backup-progress-flush; then
    ui_capture progress-failed '(Tape content/position uncertain; inspect.|Bandinhalt/Position unklar; bitte pruefen.)'
    if grep -Eq '(Session written; tape closed.|Sitzung geschrieben; Band geschlossen.)' "$OUT/ui-progress-failed.txt"; then exit 1; fi
else
    ui_capture progress-complete '(Session written; tape closed.|Sitzung geschrieben; Band geschlossen.)'
    grep -Eq '(Readback verification still required.|Ruecklesepruefung weiterhin erforderlich.)' "$OUT/ui-progress-complete.txt"
    if test "$variant" = ui-backup-progress-tree-mouse; then
        grep -Eq '(Files=2 Directories=1|Dateien=2 Verzeichnisse=1)' "$OUT/ui-progress-complete.txt"
    fi
fi
ui_key b
ui_capture progress-result-consumed '(Devices and tape status|Geraete und Bandstatus)'
ui_key b
ui_capture progress-fresh-grant '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
ui_key esc
ui_capture progress-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

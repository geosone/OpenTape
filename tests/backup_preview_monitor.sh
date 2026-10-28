source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
ui_capture preflight-start '(Devices and tape status|Geraete und Bandstatus)'
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 21; else ui_key f7; fi
if test "$variant" = ui-preflight-disabled; then
    ui_capture preflight-disabled '(No source:|Quelle fehlt:)'
elif test "$variant" = ui-preflight-file || test "$variant" = ui-preflight-tree-mouse || test "$variant" = ui-preflight-depth; then
    ui_capture preflight-complete '(B plans again and needs write confirmation.|B plant neu und verlangt Schreibbestaetigung.)'
    if test "$variant" = ui-preflight-file; then
        grep -Eq '(Files=1 Directories=0|Dateien=1 Verzeichnisse=0)' "$OUT/ui-preflight-complete.txt"
        grep -Eq '(Payload: 35000 bytes|Nutzdaten: 35000 Bytes)' "$OUT/ui-preflight-complete.txt"
    elif test "$variant" = ui-preflight-depth; then
        grep -Eq '(Files=1 Directories=17|Dateien=1 Verzeichnisse=17)' "$OUT/ui-preflight-complete.txt"
        grep -Eq '(Payload: 35000 bytes|Nutzdaten: 35000 Bytes)' "$OUT/ui-preflight-complete.txt"
    else
        grep -Eq '(Files=2 Directories=2|Dateien=2 Verzeichnisse=2)' "$OUT/ui-preflight-complete.txt"
        grep -Eq '(Payload: 35020 bytes|Nutzdaten: 35020 Bytes)' "$OUT/ui-preflight-complete.txt"
    fi
else
    ui_capture preflight-failed '(Preflight FAILED|Vorpruefung FEHLER)'
    grep -Eq '(No complete source counts available.|Keine vollstaendigen Quellzaehler verfuegbar.)' "$OUT/ui-preflight-failed.txt"
    if test "$variant" = ui-preflight-limit; then grep -Fq 'rc=-110' "$OUT/ui-preflight-failed.txt"; fi
fi
# B only dismisses this result, even after successful planning.
ui_key b
ui_capture preflight-result-consumed '(Devices and tape status|Geraete und Bandstatus)'
ui_key b
if test "$variant" = ui-preflight-disabled; then
    ui_capture preflight-no-source '(No source:|Quelle fehlt:)'
else
    ui_capture preflight-no-grant '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
fi
ui_key esc
ui_capture preflight-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

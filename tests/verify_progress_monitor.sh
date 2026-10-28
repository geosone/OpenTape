# Read pauses are fixture-only. Both V and L must observe the existing read
# sequence, without additional status/device submissions or filesystem writes.
source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
v_confirm() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
    elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
}
v_rewind() {
    ui_key r
    ui_capture vprogress-rewind '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
    v_confirm
    ui_capture vprogress-rewound '(Rewound; query status again|Zurueckgespult; Status neu abfragen)'
}
v_stream() {
    local phase=$1
    ui_capture "vprogress-$phase-partial" '(File bytes: 32712/35000|Dateibytes: 32712/35000)'
    grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-vprogress-$phase-partial.txt"
    grep -Eq '(Archive payload checked: 32712 bytes|Archiv-Nutzdaten geprueft: 32712 Bytes)' "$OUT/ui-vprogress-$phase-partial.txt"
    grep -Eq '(Checked files: 0; dirs:|Dateien: 0; Ordner:)' "$OUT/ui-vprogress-$phase-partial.txt"
    if test "$phase" = catalog; then
        grep -Eq '(Catalogue provisional; not available yet|Katalog vorlaeufig; noch nicht verfuegbar)' "$OUT/ui-vprogress-$phase-partial.txt"
    fi
    ui_capture "vprogress-$phase-full" '(File bytes: 35000/35000|Dateibytes: 35000/35000)'
    grep -Eq '(Checked files: 0; dirs:|Dateien: 0; Ordner:)' "$OUT/ui-vprogress-$phase-full.txt"
    ui_capture "vprogress-$phase-completed-file" '(Checked files: 1; dirs:|Dateien: 1; Ordner:)'
    if test "$variant" = ui-verify-progress-tree; then
        ui_capture "vprogress-$phase-tree" '(Archive payload checked: 35020 bytes|Archiv-Nutzdaten geprueft: 35020 Bytes)'
        grep -Fq 'DATA/NOTE.TXT' "$OUT/ui-vprogress-$phase-tree.txt"
    fi
    if test "$variant" != ui-verify-progress-late || test "$phase" != catalog; then
        ui_capture "vprogress-$phase-boundary" '(Filemark and tape close still pending|Dateimarke und Bandschliessen stehen noch aus)'
        grep -Eq '(Provisional; filemark/tape close pending|Vorlaeufig bis Dateimarke und Bandschliessen)' "$OUT/ui-vprogress-$phase-boundary.txt"
    fi
}
ui_capture vprogress-start '(Devices and tape status|Geraete und Bandstatus)'
v_rewind
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 17; else ui_key v; fi
ui_capture vprogress-verify-confirm '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
v_confirm
v_stream verify
ui_capture vprogress-verified '(Archive verification OK; tape closed|Archivpruefung OK; Band geschlossen)'
v_rewind
if test "$variant" = ui-verify-progress-late; then v_rewind; fi
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 19; else ui_key l; fi
ui_capture vprogress-catalog-confirm '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
v_confirm
v_stream catalog
if test "$variant" = ui-verify-progress-late; then
    ui_capture vprogress-failed '(Verification FAILED|Pruefung FEHLER)'
    grep -Eq '(INCOMPLETE; counters diagnostic only|UNVOLLSTAENDIG; Zaehler nur Diagnose)' "$OUT/ui-vprogress-failed.txt"
    ui_key f8
    ui_capture vprogress-discarded '(Devices and tape status|Geraete und Bandstatus)'
else
    ui_capture vprogress-catalog '(Verified archive contents|Gepruefter Archivinhalt)'
    grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-vprogress-catalog.txt"
    ui_key b
    ui_capture vprogress-back '(Devices and tape status|Geraete und Bandstatus)'
fi
ui_key esc

# Read latency starts at the third rewind, after verified catalogue loading.
# Counts distinguish archive scanning from selected-file output/comparison.
source "$ROOT/tests/mouse_monitor_helpers.sh"
if test "$variant" = ui-restore-progress-tree; then ui_key down; sleep 0.15; fi
ui_key spc
if test "$variant" = ui-compare-progress || test "$variant" = ui-preview-progress; then ui_key o; fi
ui_capture rprogress-marked '(Marked: 1 files|Auswahl: 1 Dateien)'
if test "$variant" = ui-compare-progress; then action=k
elif test "$variant" = ui-preview-progress; then action=c
else action=w; fi
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 20; else ui_key "$action"; fi
ui_capture rprogress-confirm '(REALLY restore 1 selected files|1 Dateien WIRKLICH wiederherstellen|Inspect 1 selected files|1 Dateien pruefen|Compare 1 selected files|1 Dateien mit dem Archiv vergleichen)'
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture rprogress-partial '(File bytes: 32712/35000|Dateibytes: 32712/35000)'
grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-rprogress-partial.txt"
grep -Eq '(Archive payload checked: 32712 bytes|Archiv-Nutzdaten geprueft: 32712 Bytes)' "$OUT/ui-rprogress-partial.txt"
grep -Eq '(Files restored: 0/1|Files compared: 0/1|Files planned: 0/1|Dateien wiederhergestellt: 0/1|Dateien verglichen: 0/1|Dateien geplant: 0/1)' "$OUT/ui-rprogress-partial.txt"
ui_capture rprogress-file-full '(File bytes: 35000/35000|Dateibytes: 35000/35000)'
grep -Eq '(Files restored: 0/1|Files compared: 0/1|Files planned: 0/1|Dateien wiederhergestellt: 0/1|Dateien verglichen: 0/1|Dateien geplant: 0/1)' "$OUT/ui-rprogress-file-full.txt"
if test "$variant" = ui-restore-progress-late; then
    ui_capture rprogress-published '(Files restored: 1/1|Dateien wiederhergestellt: 1/1)'
    ui_capture rprogress-late-failed '(Restore FAILED|Restore FEHLER)'
    grep -Eq '(Readback=1 rc=0 Recovery=0|Ruecklesen=1 rc=0 Recovery=0)' "$OUT/ui-rprogress-late-failed.txt"
    if grep -Eq '(Restore OK;|Vergleich OK;|Compare OK;)' "$OUT/ui-rprogress-late-failed.txt"; then exit 1; fi
else
    if test "$variant" = ui-restore-progress-tree; then
        ui_capture rprogress-excluded '(Not selected; archive data is still verified.|Nicht ausgewaehlt; Archivdaten werden geprueft.)'
        grep -Fq 'DATA/NOTE.TXT' "$OUT/ui-rprogress-excluded.txt"
        ui_capture rprogress-tree-bytes '(Archive payload checked: 35020 bytes|Archiv-Nutzdaten geprueft: 35020 Bytes)'
    fi
    ui_capture rprogress-boundary '(Filemark and tape close still pending.|Dateimarke und Bandschliessen stehen noch aus.)'
    grep -Eq '(Provisional; filemark/tape close pending.|Vorlaeufig bis Dateimarke und Bandschliessen.)' "$OUT/ui-rprogress-boundary.txt"
    ui_capture rprogress-result '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.|Preview OK; tape closed.|Vorschau OK; Band geschlossen.|Compare OK; archive and files checked.|Vergleich OK; Archiv und Dateien geprueft.)'
fi
ui_key esc
ui_capture rprogress-back '(Marked: 1 files|Auswahl: 1 Dateien)'
if test "$variant" = ui-preview-progress || test "$variant" = ui-compare-progress; then
    grep -Eq '(replace: 1|Ersetzen: 1)' "$OUT/ui-rprogress-back.txt"
fi
ui_key b
ui_capture rprogress-device '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

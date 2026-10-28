# Catalogue P preserves marks and replacement intent while each empty field
# disables only its action. Editing is blocked by an outstanding confirmation.
ui_key spc; ui_key o
ui_capture paths-grant '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_key c
ui_capture paths-pending '(Inspect 1 selected files|1 Dateien pruefen)'
ui_key p
ui_capture paths-p-cancels '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_key p
ui_capture paths-catalogue-edit '(Catalogue paths|Katalog-Pfade)'
for ((field=0; field<5; field++)); do
    ui_key home; ui_key shift-end; sleep 0.15; ui_key backspace; sleep 0.15
    ui_key tab; sleep 0.15
done
ui_key f2
ui_capture paths-disabled-saved '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
for command in c w k; do
    ui_key "$command"
    ui_capture "paths-disabled-$command" '(No target:|Ziel fehlt:)'
    ui_key esc
    ui_capture "paths-return-$command" '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
done
for command in i e; do
    ui_key "$command"
    ui_capture "paths-disabled-$command" '(No source:|Quelle fehlt:|No output:|Ziel fehlt:|--selection-in|--selection-out)'
done
ui_key p
ui_capture paths-cleared '(Catalogue paths|Katalog-Pfade)'
cut -c 1-50 "$OUT/ui-paths-cleared.txt" | awk '
    NR==8 || NR==11 || NR==14 || NR==17 || NR==20 {
        gsub(/ /, ""); if (length($0) != 0) exit 1
    }'
ui_key esc
ui_capture paths-catalogue-retained '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
ui_key b
ui_capture paths-device '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

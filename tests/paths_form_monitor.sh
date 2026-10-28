# Configure via the production QUILIB controls before using the existing
# catalogue operation fixtures. No --preview/restore/compare/list arguments.
source "$ROOT/tests/mouse_monitor_helpers.sh"
paths_type() {
    local text=$1 char n
    sleep 0.15
    for ((n=0; n<${#text}; n++)); do
        char=${text:n:1}
        case "$char" in /) char=slash ;; .) char=dot ;; '*') char=shift-8 ;; esac
        printf 'sendkey %s 30\n' "$char"; sleep 0.06
    done
}
paths_open() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 10; else ui_key p; fi
    ui_capture paths-open '(Catalogue paths|Katalog-Pfade)'
}
paths_confirm() { if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi; }
paths_back() { ui_key esc; ui_capture paths-back '(Devices and tape status|Geraete und Bandstatus)'; }
ui_capture paths-start '(Devices and tape status|Geraete und Bandstatus)'
paths_open
if test "$variant" = ui-paths-cancel; then
    for value in PREVIEW RECOVER CHECK INPUT.LST UI.LST; do grep -Fq "$value" "$OUT/ui-paths-open.txt"; done
    paths_type discard
    ui_key f2; ui_capture paths-temporary-save '(Devices and tape status|Geraete und Bandstatus)'
    paths_open
    grep -Fiq discard "$OUT/ui-paths-open.txt"
    ui_key home; ui_key shift-end; paths_type '*.bin'
    ui_key f2; ui_capture paths-invalid '(Invalid path:|Ungueltiger Pfad:)'
    paths_back
    paths_open
    grep -Fiq discard "$OUT/ui-paths-open.txt"
    if grep -Fq '*.bin' "$OUT/ui-paths-open.txt"; then exit 1; fi
    ui_key home; ui_key shift-end; paths_type changed
    paths_back
    paths_open
    grep -Fiq discard "$OUT/ui-paths-open.txt"
    if grep -Fiq changed "$OUT/ui-paths-open.txt"; then exit 1; fi
    for value in RECOVER CHECK INPUT.LST UI.LST; do grep -Fq "$value" "$OUT/ui-paths-open.txt"; done
    paths_back
    ui_key esc
    return 0
fi
values=(preview recover recover input.lst ui.lst)
for ((field=0; field<5; field++)); do
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 4 $((7+3*field)); fi
    paths_type "${values[field]}"
    if [[ "$variant" != *mouse* ]]; then ui_key tab; sleep 0.15; fi
done
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 7 21; else ui_key f2; fi
ui_capture paths-saved '(Devices and tape status|Geraete und Bandstatus)'
paths_open
for value in preview recover input.lst ui.lst; do grep -Fiq "$value" "$OUT/ui-paths-open.txt"; done
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 20 21; else ui_key esc; fi
ui_capture paths-configured '(Devices and tape status|Geraete und Bandstatus)'

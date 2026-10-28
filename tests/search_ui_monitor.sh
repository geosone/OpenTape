# Sourced after verified tree catalogue. All search actions are offline;
# exact ASPI totals and the whole DOS image must stay unchanged.
source "$ROOT/tests/mouse_monitor_helpers.sh"
search_type() {
    local text=$1 char n
    sleep 0.15
    for ((n=0; n<${#text}; n++)); do
        char=${text:n:1}
        if [[ "$char" = [A-Z] ]]; then char=shift-${char,,}; fi
        printf 'sendkey %s 30\n' "$char"; sleep 0.06
    done
}
search_open() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 6; else ui_key s; fi
    ui_capture search-dialog '(Catalogue search|Katalogsuche)'
}
search_apply() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 7 21; else ui_key f2; fi
}
search_cancel() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 20 21; else ui_key esc; fi
}
search_next() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 8; else ui_key f3; fi
}
search_prev() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 68 8; else ui_key shift-f3; fi
}
search_check_marks() {
    grep -Eq '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)' "$OUT/ui-$1.txt"
    grep -Eq '\[!\].*INPUT.BIN' "$OUT/ui-$1.txt"
}
ui_key down; sleep 0.15; ui_key spc; sleep 0.15; ui_key o
ui_capture search-marked '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
# A search opener at a pending operation confirmation only cancels it.
ui_key c
ui_capture search-pending '(Inspect 1 selected files|1 Dateien pruefen)'
ui_key s
ui_capture search-pending-cancel '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
# Displace path inspection; a later search hit resets the detail offset.
ui_key right
search_open
search_type NoTe
search_apply
ui_capture search-note '(Match 3/3;|Treffer 3/3;)'
grep -Fq '000: DATA/NOTE.TXT' "$OUT/ui-search-note.txt"
search_check_marks search-note
search_next
ui_capture search-single-forward '(search wrapped|Suche hat umgebrochen)'
search_prev
ui_capture search-single-back '(search wrapped|Suche hat umgebrochen)'
search_open
search_type data
search_apply
ui_capture search-current '(Match 3/3; F3|Treffer 3/3; F3)'
search_next
ui_capture search-wrap-first '(Match 1/3; search wrapped|Treffer 1/3; Suche hat umgebrochen)'
grep -Fq '000: DATA' "$OUT/ui-search-wrap-first.txt"
search_check_marks search-wrap-first
search_next
ui_capture search-second '(Match 2/3; F3|Treffer 2/3; F3)'
search_prev
ui_capture search-first-back '(Match 1/3; F3|Treffer 1/3; F3)'
search_prev
ui_capture search-wrap-back '(Match 3/3; search wrapped|Treffer 3/3; Suche hat umgebrochen)'
search_open
search_type absent
search_apply
ui_capture search-missing '(No matching catalogue entry.|Kein passender Katalogeintrag.)'
grep -Fq '000: DATA/NOTE.TXT' "$OUT/ui-search-missing.txt"
search_check_marks search-missing
search_next
ui_capture search-missing-next '(No matching catalogue entry.|Kein passender Katalogeintrag.)'
search_open
search_type discarded
search_cancel
ui_capture search-draft-cancel '(No matching catalogue entry.|Kein passender Katalogeintrag.)'
search_open
grep -Eiq '^[[:space:]]+absent[[:space:]]+$' "$OUT/ui-search-dialog.txt"
search_type input
search_apply
ui_capture search-input '(Match 2/3; search wrapped|Treffer 2/3; Suche hat umgebrochen)'
grep -Fq '000: DATA/INPUT.BIN' "$OUT/ui-search-input.txt"
search_open
ui_key home; ui_key shift-end; sleep 0.15; ui_key backspace; sleep 0.15
search_apply
ui_capture search-cleared '(No search text;|Kein Suchtext;)'
search_next
ui_capture search-empty-next '(No search text;|Kein Suchtext;)'
search_open
search_type qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq
ui_key ins; ui_key end; ui_key x
ui_capture search-limit '(Field length limit reached|Feldgrenze erreicht)'
search_apply
ui_capture search-long-missing '(No matching catalogue entry.|Kein passender Katalogeintrag.)'
search_open
grep -Eq '^[[:space:]]+q{40}[[:space:]]+$' "$OUT/ui-search-dialog.txt"
search_cancel
ui_capture search-final '(No matching catalogue entry.|Kein passender Katalogeintrag.)'
search_check_marks search-final
ui_key b
ui_capture search-device '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

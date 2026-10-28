# Live catalogue baseline was verified above. Every operation below is disk/RAM
# only; the fixture's exact 28 CDB total detects accidental tape access.
source "$ROOT/tests/mouse_monitor_helpers.sh"
cat_type() {
    local char n
    sleep 0.15
    for ((n=0; n<${#1}; ++n)); do
        char=${1:n:1}
        if [[ "$char" = [A-Z] ]]; then char=shift-${char,,}; fi
        case "$char" in .) char=dot ;; /) char=slash ;; esac
        printf 'sendkey %s 30\n' "$char"; sleep 0.06
    done
}
cat_open() {
    if [[ "$variant" = *mouse* ]]; then
        if test "$1" = save; then ui_mouse_click 70 13; else ui_mouse_click 68 4; fi
    elif test "$1" = save; then ui_key f8; else ui_key f9; fi
    ui_capture catfile-editor '(Catalogue file:|Katalogdatei:)'
}
cat_next() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 7 21; else ui_key f2; fi
    ui_capture catfile-confirm '(Perform this catalogue file operation now|Diesen Katalogvorgang jetzt ausfuehren)'
}
cat_confirm() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
    elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
}
cat_back() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 5; else ui_key esc; fi
}
ui_key spc
ui_key o
ui_capture catfile-grants '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
if test "$variant" = ui-catfile || test "$variant" = ui-catfile-mouse; then
    cat_open save
    cat_type CANCEL.CAT
    ui_key esc
    ui_capture catfile-cancel-editor '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    cat_open save
    grep -Fq OPENTAPE.CAT "$OUT/ui-catfile-editor.txt"
    cat_next
    # The import key cancels pending export; it cannot start another operation.
    ui_key f9
    ui_capture catfile-cancel-confirm '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    cat_open save; cat_next; cat_confirm
    ui_capture catfile-saved '(Metadata checked and saved|Metadaten geprueft und gespeichert)'
    cat_back
    ui_capture catfile-live '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    cat_open save; cat_next; cat_confirm
    ui_capture catfile-existing '(Catalogue file FAILED|Katalogdatei FEHLER)'
    grep -Fq 'DOS=80' "$OUT/ui-catfile-existing.txt"
    cat_back
    ui_capture catfile-existing-back '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    # Leave live view, then load from the device panel (mouse F9 target).
    ui_key b
    ui_capture catfile-device '(Devices and tape status|Geraete und Bandstatus)'
    cat_open load; cat_next; cat_confirm
    ui_capture catfile-loaded '(Offline metadata loaded|Offline-Metadaten geladen)'
    cat_back
    ui_capture catfile-offline '(Saved catalogue: OFFLINE|Gespeicherter Katalog: OFFLINE)'
    grep -Eq '(Marked: 0 files; replace: 0|Auswahl: 0 Dateien; Ersetzen: 0)' "$OUT/ui-catfile-offline.txt"
    grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-catfile-offline.txt"
    ui_key spc
    ui_capture catfile-select '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
    for key in o c w k; do
        ui_key "$key"
        ui_capture "catfile-block-$key" '(Offline: first read live catalogue|Offline: erst mit L Katalog vom Band lesen)'
        grep -Eq '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)' "$OUT/ui-catfile-block-$key.txt"
    done
    ui_key n
    ui_capture catfile-clear '(Marked: 0 files; replace: 0|Auswahl: 0 Dateien; Ersetzen: 0)'
elif test "$variant" = ui-catfile-alias; then
    cat_open load; cat_type RMT0.CAT; ui_key f2
    ui_capture catfile-alias '(Invalid or empty catalogue filename|Ungueltiger oder leerer Katalog-Dateiname)'
    ui_key esc
    ui_capture catfile-alias-preserved '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
else
    if test "$variant" = ui-catfile-staging; then cat_open save
    else cat_open load
        if test "$variant" = ui-catfile-corrupt; then cat_type BROKEN.CAT; fi
    fi
    cat_next; cat_confirm
    ui_capture catfile-failed '(Catalogue file FAILED|Katalogdatei FEHLER)'
    cat_back
    ui_capture catfile-preserved '(Marked: 1 files; replace: 1|Auswahl: 1 Dateien; Ersetzen: 1)'
    grep -Eq '(Verified archive contents|Gepruefter Archivinhalt)' "$OUT/ui-catfile-preserved.txt"
fi
ui_key b
ui_capture catfile-exit '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

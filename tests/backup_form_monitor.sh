# Real QUILIB editor, keyboard plus installed INT33 mouse in mouse variants.
# Saving/cancelling alone is verified by zero CDB counts and unchanged disk.
source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
form_type() {
    local text=$1 char
    for ((n=0; n<${#text}; n++)); do
        char=${text:n:1}
        case "$char" in
            /) char=slash ;; .) char=dot ;; '*') char=shift-8 ;; '!') char=shift-1 ;;
        esac
        ui_key "$char"; sleep 0.04
    done
}
form_open() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 12; else ui_key f; fi
    ui_capture form-open '(Backup settings|Backup-Einstellungen)'
}
form_next() { ui_key tab; sleep 0.15; }
form_save() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 7 21; else ui_key f2; fi
}
form_cancel() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 20 21; else ui_key esc; fi
    ui_capture form-cancelled '(Devices and tape status|Geraete und Bandstatus)'
}
ui_capture form-start '(Devices and tape status|Geraete und Bandstatus)'
form_open
case "$variant" in
    ui-backup-form-cancel)
        grep -Fq 'INPUT.BIN' "$OUT/ui-form-open.txt"
        form_type none.bin; form_next; form_type other; form_next; form_type discard
        ui_key f4; form_cancel
        form_open
        grep -Fq 'INPUT.BIN' "$OUT/ui-form-open.txt"
        grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-form-open.txt"
        grep -Fq 'TEST' "$OUT/ui-form-open.txt"
        grep -Eq '(Single-file backup|Einzeldatei-Backup)' "$OUT/ui-form-open.txt"
        if grep -Eiq '(none.bin|discard)' "$OUT/ui-form-open.txt"; then exit 1; fi
        form_cancel ;;
    ui-backup-form-invalid)
        form_save
        ui_capture form-empty '(Invalid source|Ungueltige Quelle)'
        form_type '*.bin'; form_save
        ui_capture form-wildcard '(Invalid source|Ungueltige Quelle)'
        ui_key home; ui_key shift-end; form_type input.bin
        form_next; form_type ../bad; form_save
        ui_capture form-path '(Invalid source|Ungueltige Quelle)'
        ui_key home; ui_key shift-end; form_type data/input.bin
        form_next; form_type 'bad!'; form_save
        ui_capture form-volume '(Invalid source|Ungueltige Quelle)'
        form_cancel
        ui_key b; ui_capture form-not-committed '(No source:|Quelle fehlt:)'; ui_key esc
        ui_capture form-disabled-back '(Devices and tape status|Geraete und Bandstatus)' ;;
    ui-backup-form-limit-mouse)
        form_type ssssssssssssssssssssssssssssssssssssssss
        ui_key ins; ui_key end; ui_key x
        ui_capture form-source-limit '(Field length limit reached|Feldgrenze erreicht)'
        form_next; form_type abcdefgh/abcdefgh/abcdefgh/abcdefgh/abcd
        ui_key ins; ui_key end; ui_key x
        ui_capture form-path-limit '(Field length limit reached|Feldgrenze erreicht)'
        form_next; form_type vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
        ui_key ins; ui_key end; ui_key x
        ui_capture form-volume-limit '(Field length limit reached|Feldgrenze erreicht)'
        # Click well past EOF of the short replacement. Backspace must edit
        # the last real character instead of accessing the blank tail.
        ui_key home; ui_key shift-end; form_type test
        ui_mouse_click 43 16; ui_key delete; sleep 0.15; ui_key backspace; sleep 0.15; ui_key t
        ui_capture form-tail-clamped '^[[:space:]]+test[[:space:]]+$'
        form_save
        ui_capture form-limit-saved '(Devices and tape status|Geraete und Bandstatus)'
        ui_key b
        ui_capture form-limit-review '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
        grep -Fiq 'ssssssssssssssssssssssssssssssssssssssss' "$OUT/ui-form-limit-review.txt"
        grep -Fiq 'abcdefgh/abcdefgh/abcdefgh/abcdefgh/abcd' "$OUT/ui-form-limit-review.txt"
        grep -Eixq '[[:space:]]*test[[:space:]]*' "$OUT/ui-form-limit-review.txt"
        ui_key esc
        ui_capture form-limit-back '(Devices and tape status|Geraete und Bandstatus)'
        form_open; form_cancel ;;
    *)
        if test "$variant" = ui-backup-form-tree-mouse; then
            ui_mouse_click 16 8
            ui_capture form-directory '(Directory backup including|Verzeichnisbackup mit)'
            ui_mouse_click 4 10; form_type src
            ui_mouse_click 4 13; ui_key home; ui_key shift-end; form_type data
            ui_mouse_click 4 16; ui_key home; ui_key shift-end; form_type test
        else
            form_type input.bin; form_next; form_type data/input.bin; form_next; form_type test
            # Reverse tab preserves edits and returns to the path field.
            ui_key shift-tab; sleep 0.15; ui_key tab
        fi
        form_save
        ui_capture form-saved '(Devices and tape status|Geraete und Bandstatus)'
        # Review persisted values, then abandon a new draft before writing.
        form_open
        grep -Fiq 'test' "$OUT/ui-form-open.txt"
        form_cancel
        if [[ "$variant" = *mouse* ]]; then ui_mouse_click 60 14; else ui_key b; fi
        ui_capture form-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
        if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
        elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        ui_capture form-written '(Session written; tape closed.|Sitzung geschrieben; Band geschlossen.)'
        if test "$variant" = ui-backup-form-tree-mouse; then
            grep -Eq '(Files=2 Directories=1|Dateien=2 Verzeichnisse=1)' "$OUT/ui-form-written.txt"
        fi
        ui_key esc
        ui_capture form-result-back '(Devices and tape status|Geraete und Bandstatus)'
        ui_key b
        ui_capture form-new-grant '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
        ui_key esc
        ui_capture form-final-back '(Devices and tape status|Geraete und Bandstatus)' ;;
esac
ui_key esc

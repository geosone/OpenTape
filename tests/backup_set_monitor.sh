# Production QUILIB form, new Set control, read-only planning, v2 profile and
# independently confirmed write. Post-UI CLI comparison checks tape contents.
source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
set_type() {
    local char n
    sleep 0.15
    for ((n=0; n<${#1}; ++n)); do
        char=${1:n:1}; if [[ "$char" = [A-Z] ]]; then char=shift-${char,,}; fi
        case "$char" in .) char=dot ;; /) char=slash ;; esac
        printf 'sendkey %s 30\n' "$char"; sleep 0.06
    done
}
set_confirm() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
    elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
}
ui_capture set-start '(Devices and tape status|Geraete und Bandstatus)'
ui_key f
ui_capture set-form '(Backup settings|Backup-Einstellungen)'
set_type SET.LST; ui_key tab; set_type DATA; ui_key tab; set_type TEST
if [[ "$variant" = *mouse* ]]; then ui_mouse_click 25 8
else
    ui_key f4
    ui_capture set-tree '(Directory backup including|Verzeichnisbackup mit)'
    ui_key f4
fi
ui_capture set-mode '(Backup set from source manifest|Backup-Set aus Quellenliste)'
ui_key f2
ui_capture set-configured '(Devices and tape status|Geraete und Bandstatus)'
ui_key f7
if test "$variant" = ui-set-depth-limit; then
    ui_capture set-preflight-limit '(Preflight FAILED rc=-110|Vorpruefung FEHLER rc=-110)'
    ui_key esc
    ui_capture set-limit-back '(Devices and tape status|Geraete und Bandstatus)'
    ui_key b
    ui_capture set-limit-confirm '(OVERWRITES at CURRENT|UEBERSCHREIBT ab AKTUELLER)'
    set_confirm
    ui_capture set-limit-stopped '(No tape output opened|Keine Bandausgabe geoeffnet)'
    ui_key esc
    ui_capture set-limit-done '(Devices and tape status|Geraete und Bandstatus)'
    ui_key esc
    return 0
elif test "$variant" = ui-set-depth; then
    ui_capture set-preflight '(Files=1 Directories=17|Dateien=1 Verzeichnisse=17)'
else
    ui_capture set-preflight '(Files=2 Directories=2|Dateien=2 Verzeichnisse=2)'
    grep -Eq '(Payload: 35020 bytes|Nutzdaten: 35020 Bytes)' "$OUT/ui-set-preflight.txt"
fi
ui_key esc
ui_capture set-after-preview '(Devices and tape status|Geraete und Bandstatus)'
# v2 profile owns the Set mode; loading it grants no write.
ui_key f5
ui_capture set-profile-name '(Profile file:|Profildatei:)'
ui_key f2
ui_capture set-profile-confirm '(Perform this profile operation now|Diesen Profilvorgang jetzt ausfuehren)'
set_confirm
ui_capture set-profile-saved '(Profile read back and exported|Profil geprueft und exportiert)'
ui_key esc
ui_capture set-after-save '(Devices and tape status|Geraete und Bandstatus)'
ui_key f
ui_capture set-change-form '(Backup set from source manifest|Backup-Set aus Quellenliste)'
ui_key f4
ui_capture set-change-file '(Single-file backup|Einzeldatei)'
ui_key f2
ui_capture set-changed '(Devices and tape status|Geraete und Bandstatus)'
ui_key f6
ui_capture set-import-name '(Profile file:|Profildatei:)'
ui_key f2
ui_capture set-import-confirm '(Perform this profile operation now|Diesen Profilvorgang jetzt ausfuehren)'
set_confirm
ui_capture set-imported '(Complete profile checked and applied|Profil vollstaendig geprueft und uebernommen)'
ui_key esc
ui_capture set-profile-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key b
ui_capture set-write-confirm '(Backup set from source manifest|Backup-Set aus Quellenliste)'
grep -Eq '(OVERWRITES at CURRENT|UEBERSCHREIBT ab AKTUELLER)' "$OUT/ui-set-write-confirm.txt"
set_confirm
if test "$variant" = ui-set-progress; then
    ui_capture set-progress '(File bytes: 32712/35000|Dateibytes: 32712/35000)'
    grep -Eq '(Payload accepted: 32712/35020|Nutzdaten: 32712/35020)' "$OUT/ui-set-progress.txt"
    grep -Fq 'DATA/SINGLE.BIN' "$OUT/ui-set-progress.txt"
    ui_capture set-finalizing '(Finalizing session|Sitzung wird abgeschlossen)'
fi
ui_capture set-written '(Session written; tape closed|Sitzung geschrieben; Band geschlossen)'
if test "$variant" = ui-set-depth; then
    grep -Eq '(Files=1 Directories=17|Dateien=1 Verzeichnisse=17)' "$OUT/ui-set-written.txt"
else
    grep -Eq '(Files=2 Directories=2|Dateien=2 Verzeichnisse=2)' "$OUT/ui-set-written.txt"
fi
ui_key b
ui_capture set-result-consumed '(Devices and tape status|Geraete und Bandstatus)'
ui_key b
ui_capture set-new-confirm '(OVERWRITES at CURRENT|UEBERSCHREIBT ab AKTUELLER)'
ui_key esc
ui_capture set-done '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

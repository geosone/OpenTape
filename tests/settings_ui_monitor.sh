# Device-only profile actions must issue zero ASPI commands. Text fields and
# confirmations use the production QUILIB input path (PS/2 in mouse variant).
source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
profile_type() {
    local text=$1 char n
    sleep 0.15
    for ((n=0; n<${#text}; n++)); do
        char=${text:n:1}
        if [[ "$char" = [A-Z] ]]; then char=shift-${char,,}; fi
        case "$char" in .) char=dot ;; /) char=slash ;; esac
        printf 'sendkey %s 30\n' "$char"; sleep 0.06
    done
}
profile_open() {
    local exporting=$1
    if [[ "$variant" = *mouse* ]]; then
        if test "$exporting" = 1; then ui_mouse_click 60 6; else ui_mouse_click 60 8; fi
    elif test "$exporting" = 1; then ui_key f5; else ui_key f6; fi
    ui_capture profile-edit '(Profile file:|Profildatei:)'
}
profile_next() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 7 21; else ui_key f2; fi
    ui_capture profile-confirm '(Perform this profile operation now|Diesen Profilvorgang jetzt ausfuehren)'
}
profile_confirm() {
    if [[ "$variant" = *mouse* ]]; then ui_mouse_click 5 21
    elif test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
}
profile_result_back() {
    ui_key f5
    ui_capture profile-result-back '(Devices and tape status|Geraete und Bandstatus)'
}
profile_check_presets() {
    ui_key f
    ui_capture profile-backup '(Backup settings|Backup-Einstellungen)'
    for value in INPUT.BIN DATA/INPUT.BIN TEST; do grep -Fq "$value" "$OUT/ui-profile-backup.txt"; done
    grep -Eq '(Single-file backup|Einzeldatei-Backup)' "$OUT/ui-profile-backup.txt"
    ui_key esc
    ui_capture profile-backup-back '(Devices and tape status|Geraete und Bandstatus)'
    ui_key p
    ui_capture profile-paths '(Catalogue paths|Katalog-Pfade)'
    for value in PREVIEW RECOVER CHECK INPUT.LST UI.LST; do grep -Fq "$value" "$OUT/ui-profile-paths.txt"; done
    ui_key esc
    ui_capture profile-paths-back '(Devices and tape status|Geraete und Bandstatus)'
}
ui_capture profile-start '(Devices and tape status|Geraete und Bandstatus)'
# Configure through F/P rather than exceeding DOS's 127-byte command tail.
ui_key f
ui_capture profile-setup-backup '(Backup settings|Backup-Einstellungen)'
profile_type INPUT.BIN; ui_key tab; profile_type DATA/INPUT.BIN; ui_key tab; profile_type TEST
ui_key f2
ui_capture profile-setup-backup-done '(Devices and tape status|Geraete und Bandstatus)'
ui_key p
ui_capture profile-setup-paths '(Catalogue paths|Katalog-Pfade)'
for value in PREVIEW RECOVER CHECK INPUT.LST UI.LST; do profile_type "$value"; ui_key tab; sleep 0.15; done
ui_key f2
ui_capture profile-setup-done '(Devices and tape status|Geraete und Bandstatus)'
if test "$variant" = ui-profile || test "$variant" = ui-profile-mouse; then
    # Discarded filename must not replace the remembered filename or create output.
    profile_open 1
    profile_type cancel.cfg
    ui_key esc
    ui_capture profile-cancelled '(Devices and tape status|Geraete und Bandstatus)'
    profile_open 1
    grep -Fq OPENTAPE.CFG "$OUT/ui-profile-edit.txt"
    profile_next
    if [[ "$variant" = *mouse* ]]; then
        ui_mouse_click 60 6
        ui_capture profile-opener-not-confirm '(Perform this profile operation now|Diesen Profilvorgang jetzt ausfuehren)'
    fi
    ui_key esc
    ui_capture profile-confirm-cancel '(Devices and tape status|Geraete und Bandstatus)'
    profile_open 1; profile_next; profile_confirm
    ui_capture profile-exported '(Profile read back and exported.|Profil geprueft und exportiert.)'
    profile_result_back
    # A fresh export refuses the existing file before acquiring staging output.
    profile_open 1; profile_next; profile_confirm
    ui_capture profile-existing '(Profile FAILED|Profil FEHLER)'
    grep -Fq 'DOS=80' "$OUT/ui-profile-existing.txt"
    profile_result_back
    ui_key f
    ui_capture profile-change-backup '(Backup settings|Backup-Einstellungen)'
    profile_type none.bin; ui_key tab; profile_type other; ui_key tab; profile_type modified
    ui_key f4; ui_key f2
    ui_capture profile-backup-modified '(Devices and tape status|Geraete und Bandstatus)'
    ui_key p
    ui_capture profile-change-paths '(Catalogue paths|Katalog-Pfade)'
    for value in changed missing newroot new.lst new2.lst; do profile_type "$value"; ui_key tab; sleep 0.15; done
    ui_key f2
    ui_capture profile-paths-modified '(Devices and tape status|Geraete und Bandstatus)'
    profile_open 0; profile_next; profile_confirm
    ui_capture profile-imported '(Complete profile checked and applied.|Profil vollstaendig geprueft und uebernommen.)'
    profile_result_back
    profile_check_presets
    # Loading settings does not grant the backup operation; B still needs Y/J.
    ui_key b
    ui_capture profile-no-grant '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
    grep -Fq INPUT.BIN "$OUT/ui-profile-no-grant.txt"
    ui_key esc
    ui_capture profile-back '(Devices and tape status|Geraete und Bandstatus)'
elif test "$variant" = ui-profile-device; then
    profile_open 0
    profile_type RMT0.CFG
    ui_key f2
    ui_capture profile-device-refused '(Invalid or empty profile filename.|Ungueltiger oder leerer Profil-Dateiname.)'
    ui_key esc
    ui_capture profile-device-back '(Devices and tape status|Geraete und Bandstatus)'
    profile_check_presets
elif test "$variant" = ui-profile-empty; then
    profile_open 0; profile_type empty.cfg; profile_next; profile_confirm
    ui_capture profile-empty-imported '(Complete profile checked and applied.|Profil vollstaendig geprueft und uebernommen.)'
    profile_result_back
    ui_key b
    ui_capture profile-empty-disabled '(No source:|Quelle fehlt:)'
    ui_key esc
    ui_capture profile-empty-back '(Devices and tape status|Geraete und Bandstatus)'
    ui_key p
    ui_capture profile-empty-paths '(Catalogue paths|Katalog-Pfade)'
    cut -c 1-50 "$OUT/ui-profile-empty-paths.txt" | awk '
        NR==8 || NR==11 || NR==14 || NR==17 || NR==20 { gsub(/ /, ""); if (length($0)) exit 1 }'
    ui_key esc
    ui_capture profile-empty-done '(Devices and tape status|Geraete und Bandstatus)'
else
    if test "$variant" = ui-profile-staging; then profile_open 1
    else
        profile_open 0
        if test "$variant" = ui-profile-missing; then profile_type none.cfg; fi
    fi
    profile_next; profile_confirm
    ui_capture profile-error '(Profile FAILED|Profil FEHLER)'
    profile_result_back
    profile_check_presets
fi
ui_key esc

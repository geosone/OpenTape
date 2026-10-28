# Real VGA/HMP interaction, with command counters checked after leaving the UI.
source "$ROOT/tests/ui_monitor_helpers.sh"
ui_text() {
    local text=$1 i
    for ((i=0;i<${#text};++i)); do ui_key "${text:i:1}"; sleep 0.12; done
}
ui_confirm() { if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi; }
ui_action() {
    ui_key "$1"
    ui_capture "$2-prompt" '(Execute this command now|Diesen Befehl jetzt)'
    ui_confirm
}
ui_goto() {
    ui_key g
    ui_capture "$3-block" '(Digits, backspace|Ziffern, Ruecktaste)'
    ui_text "$1"; ui_key ret
    ui_capture "$3-partition" '(Enter = current partition|Enter = aktuelle Partition)'
    if test -n "$2"; then ui_text "$2"; fi
    ui_key ret
    ui_capture "$3-prompt" '(Execute this command now|Diesen Befehl jetzt)'
    ui_confirm
}
if test "$variant" = legacy-ui; then
    ui_capture legacy-start 'API: ASPITAPE'
    ! grep -Eq '(T Tape controls|T Bandsteuerung)' "$OUT/ui-legacy-start.txt"
    ui_key t
    ui_capture legacy-unchanged 'API: ASPITAPE'
    ui_key esc
    return 0
fi
ui_capture start '(T Tape controls|T Bandsteuerung)'
ui_key t
ui_capture panel '(Native tape controls|Native Bandsteuerung)'
grep -Eq '(P Position 64-bit|P Position 64Bit)' "$OUT/ui-panel.txt"
if [[ "$variant" = alerts-* ]]; then
    source "$ROOT/tests/native_alerts_monitor.sh"
    return 0
fi
if [[ "$variant" = space-ext-* ]]; then
    source "$ROOT/tests/native_extended_space_monitor.sh"
    return 0
fi
if [[ "$variant" = space-* ]]; then
    source "$ROOT/tests/native_space_monitor.sh"
    return 0
fi
if [[ "$variant" = layout-* ]]; then
    ui_key i
    if test "$variant" = layout-legacy; then
        ui_capture layout-unverified '(Transfer length not verified|Antwortlaenge nicht bestaetigt)'
        grep -Eq '(Result|Ergebnis) rc=-206 DOS=0' "$OUT/ui-layout-unverified.txt"
    else
        ui_capture layout-failed '(Result|Ergebnis) rc=-209 DOS=0'
        if test "$variant" = layout-host; then d=1; else d=3; fi
        grep -Eq "(Class|Klasse)=$d" "$OUT/ui-layout-failed.txt"
    fi
    ! grep -Eq '(Additional:|Zusaetzlich:|Size descriptors:|Groesseneintraege:)' "$OUT"/ui-layout-*.txt
    ui_key esc
    ui_capture layout-back '(Status: not queried|Status: nicht abgefragt)'
    ui_key esc
    return 0
fi
if test "$variant" = normal; then
    ui_key i
    ui_capture layout '(Additional: max 3, reported 2|Zusaetzlich: max 3, gemeldet 2)'
    grep -Eq '(Flags 3C / recognition 03 / units 18|Flags 3C / Erkennung 03 / Einheit 18)' "$OUT/ui-layout.txt"
    grep -Eq '(Reported layout; may be pending|Gemeldetes Layout; kann noch ausstehend)' "$OUT/ui-layout.txt"
fi
ui_key p
if test "$variant" = old-sdk; then
    ui_capture missing '(newer provider SDK|Neueres Provider-SDK)'
    ui_key i
    ui_capture missing-i '(newer provider SDK|Neueres Provider-SDK)'
    ui_key a
    ui_capture missing-a '(newer provider SDK|Neueres Provider-SDK)' 
    ui_key g
    ui_capture missing-g '(newer provider SDK|Neueres Provider-SDK)'
    ui_key k
    ui_capture missing-k '(newer provider SDK|Neueres Provider-SDK)'
    for unit in 4 5 6; do
        ui_key s
        ui_capture "old-space-$unit" '(Relative tape positioning|Relative Bandpositionierung)'
        ui_key "$unit"
        ui_capture "missing-space-$unit" '(newer provider SDK|Neueres Provider-SDK)'
    done
elif test "$variant" = unknown || test "$variant" = legacy; then
    ui_capture unknown '(Logical block position unknown|Logische Blockposition unbekannt)'
    if test "$variant" = legacy; then
        ui_capture legacy '(Transfer length not verified|Antwortlaenge nicht bestaetigt)'
    else ui_capture file '4294967303'; fi
    ui_goto 4294967296 '' unknown-target
    ui_capture target-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
    ui_key p
    ui_capture still-unknown '(Logical block position unknown|Logische Blockposition unbekannt)'
else
    ui_capture position '(Logical file identifier|Logische Dateimarkennummer)'
    # Cancel consumes one key and must not open the device or move tape.
    ui_key u
    ui_capture cancel-prompt '(Execute this command now|Diesen Befehl jetzt)'
    ui_key esc
    ui_capture cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
    ui_key g
    ui_capture edit '(Digits, backspace|Ziffern, Ruecktaste)'
    ui_key ret
    ui_capture empty '(Invalid number|Ungueltige Zahl)'
    ui_key minus; ui_key 1; sleep 0.15; ui_key ret
    ui_capture signed '(Invalid number|Ungueltige Zahl)'
    ui_key backspace
    ui_text 18446744073709551616
    ui_key ret
    ui_capture overflow '(Invalid number|Ungueltige Zahl)'
    ui_key backspace; ui_key 5; sleep 0.15
    ui_key 0; sleep 0.15
    ui_key ret
    ui_capture sticky '(Invalid number|Ungueltige Zahl)'
    grep -Eq '(Digits, backspace|Ziffern, Ruecktaste)' "$OUT/ui-sticky.txt"
    ui_key backspace; ui_key 5; sleep 0.15
    ui_key ret
    ui_capture partition '(Enter = current partition|Enter = aktuelle Partition)'
    ui_text 256; ui_key ret
    ui_capture bad-partition '(Invalid number|Ungueltige Zahl)'
    ui_key backspace; ui_key 5; sleep 0.15
    ui_key ret
    ui_capture cancelled-target-prompt '(Execute this command now|Diesen Befehl jetzt)'
    grep -Fq '18446744073709551615' "$OUT/ui-cancelled-target-prompt.txt"
    ui_key p
    ui_capture cancelled-target '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
    ui_goto 18446744073709551615 2 target
    if [[ "$variant" = host || "$variant" = deferred || "$variant" = unsupported ]]; then
        ui_capture failed '(Tape position unknown; no automatic retry|Bandposition unklar; keine Wiederholung)'
        grep -Eq "(Class|Klasse)=$diagnostic" "$OUT/ui-failed.txt"
        ui_key p
        ui_capture moved '16045690985374415566'
        grep -Eq '(Session|Sitzung): 0040' "$OUT/ui-moved.txt"
        ui_goto 0 '' blocked
        ui_capture blocked '(Result|Ergebnis) rc=-207 DOS=0'
        ui_action r recover
        ui_capture recovered '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_goto 18446744073709551615 '' retry
        ui_capture retry-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture retry-position '18446744073709551615'
        grep -Eq '(Session|Sitzung): 0000' "$OUT/ui-retry-position.txt"
    else
        ui_capture target-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture target-position '18446744073709551615'
        grep -Eq '(Partition: 2)' "$OUT/ui-target-position.txt"
        ui_goto 4294967296 0 explicit-zero
        ui_capture zero-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture zero-position '4294967296'
        grep -Fq 'Partition: 0' "$OUT/ui-zero-position.txt"
        ui_goto 9223372036854775808 '' current
        ui_capture current-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture current-position '9223372036854775808'
        ui_action k lock
        ui_capture locked '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_action u prevented
        ui_capture prevented '(Tape position unknown; no automatic retry|Bandposition unklar; keine Wiederholung)'
        grep -Fq 'ASC/ASCQ=53/02' "$OUT/ui-prevented.txt"
        ui_key p
        ui_capture uncertain '9223372036854775808'
        grep -Eq '(Session|Sitzung): 0040' "$OUT/ui-uncertain.txt"
        ui_goto 0 '' blocked
        ui_capture blocked '(Result|Ergebnis) rc=-207 DOS=0'
        ui_action o unlock
        ui_capture unlocked '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        grep -Eq '(Session|Sitzung): 0040' "$OUT/ui-unlocked.txt"
        ui_action r rewind
        ui_capture rewound '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture clean '(Session|Sitzung): 0000'
        ui_action u unload
        ui_capture unloaded '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture empty-drive '(Result|Ergebnis) rc=-209 DOS=0'
        grep -Eq '(Session|Sitzung): 0000' "$OUT/ui-empty-drive.txt"
        ui_action l load
        ui_capture loaded '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture after-load '(Session|Sitzung): 0000'
        grep -Eq '(N Retension|N Band straffen)' "$OUT/ui-after-load.txt"
        ui_action n retension
        ui_capture retensioned '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
        ui_key p
        ui_capture after-retension '(Session|Sitzung): 0000'
    fi
fi
ui_key esc
ui_capture back '(Status: not queried|Status: nicht abgefragt)'
if test "$variant" = normal; then
    ui_key 3; ui_key t
    ui_capture ordinary-file-panel '(Native tape controls|Native Bandsteuerung)'
    grep -Fq NRMT3 "$OUT/ui-ordinary-file-panel.txt"
    ui_key p
    ui_capture ordinary-file '(Result|Ergebnis) rc=-202 DOS=0'
    ui_goto 0 '' ordinary-file-move
    ui_capture ordinary-file-rejected '(Result|Ergebnis) rc=-202 DOS=0'
    ui_key esc
    ui_capture file-back '(Status: not queried|Status: nicht abgefragt)'
    ui_key 1; ui_key t
    ui_capture missing-panel '(Native tape controls|Native Bandsteuerung)'
    ui_key p
    ui_capture missing-device '(Result|Ergebnis) rc=-201 DOS=2'
    ui_key esc
    ui_capture missing-back '(Status: not queried|Status: nicht abgefragt)'
fi
ui_key esc

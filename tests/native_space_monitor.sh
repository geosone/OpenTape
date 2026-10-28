# Exercise the real signed-count editor; the provider fixture independently
# validates every SPACE opcode, direction and 24-bit count. No host tape.
space_start() {
    ui_key s
    ui_capture "$1-choice" '(Relative tape positioning|Relative Bandpositionierung)'
    grep -Eq '(S Space tape|S Band bewegen)' "$OUT/ui-$1-choice.txt"
}
space_edit() {
    space_start "$2"
    ui_key "$1"
    ui_capture "$2-edit" '(Count \(-8388608|Anzahl \(-8388608)'
    grep -Eq '(Selected: NRMT0|Ausgewaehlt: NRMT0)' "$OUT/ui-$2-edit.txt"
}
space_text() {
    local text=$1 i
    for ((i=0;i<${#text};++i)); do
        if test "${text:i:1}" = -; then ui_key minus; else ui_key "${text:i:1}"; fi
        sleep 0.12
    done
}
space_move() {
    space_edit "$1" "$3"
    space_text "$2"; ui_key ret
    ui_capture "$3-confirm" '(Execute this command now|Diesen Befehl jetzt)'
    grep -Fq -- "$2" "$OUT/ui-$3-confirm.txt"
    if [[ "$2" = -* ]]; then direction='(Direction: backward|Richtung: rueckwaerts)';
    elif test "$2" = 0; then direction='(Zero count|Anzahl null)';
    else direction='(Direction: forward|Richtung: vorwaerts)'; fi
    grep -Eq "$direction" "$OUT/ui-$3-confirm.txt"
    ui_confirm
}
space_done() {
    ui_capture "$1-done" '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
    grep -Eq '(Session|Sitzung): 0000' "$OUT/ui-$1-done.txt"
}
# Choices, editing and canceled confirmation must submit nothing. A key used
# to cancel confirmation must not become a read-position or movement command.
space_start cancel-choice; ui_key p
ui_capture choice-stays '(Relative tape positioning|Relative Bandpositionierung)'
ui_key esc
ui_capture choice-cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
space_edit 1 cancel-count; ui_key esc
ui_capture count-cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
space_edit 1 invalid
ui_key ret
ui_capture count-empty '(Invalid number|Ungueltige Zahl)'
ui_key minus; sleep 0.15; ui_key ret
ui_capture count-sign '(Invalid number|Ungueltige Zahl)'
ui_key backspace
space_text 8388608; ui_key ret
ui_capture count-overflow '(Invalid number|Ungueltige Zahl)'
ui_key esc
space_edit 1 negative-invalid
space_text -8388609; ui_key ret
ui_capture negative-overflow '(Invalid number|Ungueltige Zahl)'
ui_key backspace; ui_key 8; sleep 0.15
# A ninth character may never shorten into a valid negative minimum.
ui_key 0; sleep 0.15; ui_key ret
ui_capture count-sticky '(Invalid number|Ungueltige Zahl)'
ui_key backspace; ui_key 8; sleep 0.15; ui_key ret
ui_capture corrected-confirm '(Execute this command now|Diesen Befehl jetzt)'
grep -Fq -- '-8388608' "$OUT/ui-corrected-confirm.txt"
ui_key p
ui_capture corrected-cancel '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
space_edit 2 wrong-character
ui_key 1; ui_key x; sleep 0.15; ui_key ret
ui_capture count-character '(Invalid number|Ungueltige Zahl)'
ui_key esc
if test "$variant" = space-normal || test "$variant" = space-legacy; then
    for spec in '1 1 plus' '1 -1 minus' '1 8388607 maximum' '1 -8388608 minimum' \
                '1 0 zero' '2 2 marks-forward' '2 -2 marks-backward'; do
        read -r unit count label <<< "$spec"
        space_move "$unit" "$count" "$label"
        space_done "$label"
        grep -Eq "(Requested count|Angeforderte Anzahl): $count" "$OUT/ui-$label-done.txt"
    done
    space_start eod-cancel; ui_key 3
    ui_capture eod-cancel-confirm '(Execute this command now|Diesen Befehl jetzt)'
    grep -Eq '(Position to end of data|Zum Datenende fahren)' "$OUT/ui-eod-cancel-confirm.txt"
    ! grep -Eq '(Minus, digits|Minus, Ziffern)' "$OUT/ui-eod-cancel-confirm.txt"
    ui_key s
    ui_capture eod-cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
    space_start eod; ui_key 3
    ui_capture eod-confirm '(Execute this command now|Diesen Befehl jetzt)'
    ui_confirm; space_done eod
else
    space_move 1 1 failed
    ui_capture failed '(Tape position unknown; no automatic retry|Bandposition unklar; keine Wiederholung)'
    grep -Eq "(Class|Klasse)=$diagnostic" "$OUT/ui-failed.txt"
    grep -Eq '(Session|Sitzung): 0040' "$OUT/ui-failed.txt"
    ui_key p
    ui_capture observed '(Session|Sitzung): 0040'
    space_move 1 1 blocked
    ui_capture blocked '(Result|Ergebnis) rc=-207 DOS=0'
    ui_action r recover
    ui_capture recovered '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
    space_move 1 1 retry
    space_done retry
    ui_key p
    ui_capture clean '(Session|Sitzung): 0000'
fi
ui_key esc
ui_capture space-back '(Status: not queried|Status: nicht abgefragt)'
# Ordinary files and absent aliases reject a confirmed SPACE without CDBs.
ui_key 3; ui_key t
ui_capture file-panel '(Native tape controls|Native Bandsteuerung)'
ui_key s; ui_capture file-choice '(Relative tape positioning|Relative Bandpositionierung)'
ui_key 1; ui_capture file-count '(Count \(-8388608|Anzahl \(-8388608)'
ui_key 1; ui_key ret
ui_capture file-confirm '(Execute this command now|Diesen Befehl jetzt)'
ui_confirm; ui_capture file-rejected '(Result|Ergebnis) rc=-202 DOS=0'
ui_key esc; ui_capture file-back '(Status: not queried|Status: nicht abgefragt)'
ui_key 1; ui_key t
ui_capture missing-panel '(Native tape controls|Native Bandsteuerung)'
ui_key s; ui_capture missing-choice '(Relative tape positioning|Relative Bandpositionierung)'
ui_key 3; ui_capture missing-confirm '(Execute this command now|Diesen Befehl jetzt)'
ui_confirm; ui_capture missing-rejected '(Result|Ergebnis) rc=-201 DOS=2'
ui_key esc; ui_capture missing-back '(Status: not queried|Status: nicht abgefragt)'
ui_key esc

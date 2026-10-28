# The existing mock validates CDB codes/counts independently of these UI keys.
ext_move() {
    ui_key s
    ui_capture "$3-choice" '(Choose 1..6|1..6 waehlen)'
    grep -Eq '(4 Consecutive filemarks|4 Folge zusammenhaengender Dateimarken)' "$OUT/ui-$3-choice.txt"
    grep -Eq '(6 Consecutive setmarks|6 Folge zusammenhaengender Setmarks)' "$OUT/ui-$3-choice.txt"
    ui_key "$1"
    ui_capture "$3-count" '(Count \(-8388608|Anzahl \(-8388608)'
    grep -Eq '(Selected: NRMT0|Ausgewaehlt: NRMT0)' "$OUT/ui-$3-count.txt"
    local count=$2 i
    for ((i=0;i<${#count};++i)); do
        if test "${count:i:1}" = -; then ui_key minus; else ui_key "${count:i:1}"; fi
        sleep 0.12
    done
    ui_key ret
    ui_capture "$3-confirm" '(Execute this command now|Diesen Befehl jetzt)'
    if test "$1" = 4; then label='(Find consecutive filemarks|Folge von Dateimarken suchen)';
    elif test "$1" = 5; then label='(Space setmarks|Setmarks ueberspringen)';
    else label='(Find consecutive setmarks|Folge von Setmarks suchen)'; fi
    grep -Eq "$label" "$OUT/ui-$3-confirm.txt"
    grep -Fq -- "$2" "$OUT/ui-$3-confirm.txt"
    ui_confirm
}
if test "$variant" = space-ext-normal || test "$variant" = space-ext-legacy; then
    for unit in 4 5 6; do
        for count in 1 -1 8388607 -8388608 0; do
            label="extended-$unit-$count"
            ext_move "$unit" "$count" "$label"
            ui_capture "$label-done" '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
            grep -Eq "(Requested count|Angeforderte Anzahl): $count" "$OUT/ui-$label-done.txt"
        done
    done
else
    ext_move 5 1 fault
    ui_capture fault '(Tape position unknown; no automatic retry|Bandposition unklar; keine Wiederholung)'
    grep -Eq "(Class|Klasse)=$diagnostic" "$OUT/ui-fault.txt"
    grep -Eq '(Session|Sitzung): 0040' "$OUT/ui-fault.txt"
    ui_key p
    ui_capture observed '(Session|Sitzung): 0040'
    for unit in 4 5 6; do
        ext_move "$unit" 1 "blocked-$unit"
        ui_capture "blocked-$unit" '(Result|Ergebnis) rc=-207 DOS=0'
    done
    ui_action r recover
    ui_capture recovered '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
    ext_move 5 1 retry
    ui_capture retry-done '(Command completed; query position again|Befehl fertig; Position erneut abfragen)'
    ui_key p
    ui_capture clean '(Session|Sitzung): 0000'
fi
ui_key esc
ui_capture ext-back '(Status: not queried|Status: nicht abgefragt)'
ui_key esc

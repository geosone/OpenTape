# Explicit acknowledgment is required even though this CDB does not move tape.
# Cancellation cannot become P/R or an automatic page read.
ui_key a
ui_capture alert-cancel-prompt '(Execute this command now|Diesen Befehl jetzt)'
grep -Eq '(Reading may acknowledge drive warning flags|Lesen kann Warnflags im Laufwerk quittieren)' "$OUT/ui-alert-cancel-prompt.txt"
ui_key p
ui_capture alert-cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
ui_key a
ui_capture alert-prompt '(Execute this command now|Diesen Befehl jetzt)'
ui_confirm
case "$variant" in
    alerts-normal)
        ui_capture alerts '(Reported flags: 5, active: 3|Gemeldete Flags: 5, aktiv: 3)' ;;
    alerts-all)
        ui_capture alerts '(Reported flags: 64, active: 64|Gemeldete Flags: 64, aktiv: 64)' ;;
    alerts-clear)
        ui_capture alerts '(No active flags in the reported set|Keine aktiven Flags im gemeldeten Satz)' ;;
    alerts-empty)
        ui_capture alerts '(No flag states reported; status unknown|Keine Flag-Zustaende gemeldet; Zustand unbekannt)' ;;
    alerts-legacy)
        ui_capture alerts '(Transfer length not verified|Antwortlaenge nicht bestaetigt)'
        grep -Eq '(Result|Ergebnis) rc=-206 DOS=0' "$OUT/ui-alerts.txt" ;;
    alerts-malformed|alerts-host)
        ui_capture alerts '(Result|Ergebnis) rc=-209 DOS=0'
        if test "$variant" = alerts-host; then d=1; else d=3; fi
        grep -Eq "(Class|Klasse)=$d" "$OUT/ui-alerts.txt" ;;
esac
if test "$variant" = alerts-normal || test "$variant" = alerts-all; then
    python3 - "$OUT/ui-alerts.txt" "$variant" <<'PYCODE'
from pathlib import Path
import sys
numbers=[]
for row in Path(sys.argv[1]).read_text().splitlines():
    data=row[:54].strip()
    if data and all(c.isdigit() or c==' ' for c in data):
        numbers += [int(s) for s in data.split()]
assert numbers == (list(range(1,65)) if sys.argv[2]=='alerts-all' else [20,32,64]), numbers
PYCODE
else
    ! grep -Eq '(Active flag numbers|Aktive Flag-Nummern)' "$OUT/ui-alerts.txt"
fi
grep -Eq '(Session|Sitzung): 0000' "$OUT/ui-alerts.txt"
ui_key a
ui_capture alert-second-prompt '(Execute this command now|Diesen Befehl jetzt)'
ui_key esc
ui_capture alert-second-cancelled '(Cancelled; no command submitted|Abgebrochen; kein Befehl)'
ui_key esc
ui_capture alert-back '(Status: not queried|Status: nicht abgefragt)'
ui_key esc

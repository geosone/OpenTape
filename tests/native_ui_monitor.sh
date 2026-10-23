# Sourced with OUT=the isolated run directory; stdout is QEMU HMP.
source "$ROOT/tests/ui_monitor_helpers.sh"

if test "$variant" = ui-legacy; then
    ui_capture native-start 'API: ASPITAPE'
else
    ui_capture native-start 'API: Native OpenTape'
fi
if test "$variant" = fixed-ui; then
    ui_capture native-fixed '(fixed=512 B|fest=512 B)'
fi
ui_key d
ui_capture native-devices 'Scan 1 rc=-151'
ui_key 1
ui_key s
if test "$variant" = ui-legacy; then
    ui_capture native-missing '(Status failed rc=-2 DOS=2|Status fehlgeschlagen rc=-2 DOS=2)'
else
    ui_capture native-missing '(Status failed rc=-201 DOS=2|Status fehlgeschlagen rc=-201 DOS=2)'
fi
ui_key 3
ui_key s
if test "$variant" = ui-legacy; then
    ui_capture native-file '(Status failed rc=-3 DOS=0|Status fehlgeschlagen rc=-3 DOS=0)'
else
    ui_capture native-file '(Status failed rc=-202 DOS=0|Status fehlgeschlagen rc=-202 DOS=0)'
fi
ui_key 0
ui_key s
if test "$variant" = ui-legacy; then
    ui_capture native-status 'state=0002 block=0'
elif test "$variant" = ui-unfinished; then
    ui_capture native-unfinished '(Native state uncertain; check tape position.|Native Sitzung unklar; Position pruefen.)'
    ui_key r
    ui_capture native-recover-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
    if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
    ui_capture native-recovered '(Rewound; query status again.|Zurueckgespult; Status neu abfragen.)'
    ui_key s
    ui_capture native-clean '(Session=0000 Info=00000020|Sitzung=0000 Info=00000020)'
    ui_key esc
    return 0
else
    ui_capture native-status '(Session=0000 Info=00000020|Sitzung=0000 Info=00000020)'
fi
ui_key r
ui_capture native-cancel '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
ui_key esc
ui_capture native-cancelled '(Status path:|Statuspfad:)'
ui_key r
ui_capture native-rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
if test "$variant" = ui-failed; then
    ui_capture native-uncertain '(Tape position unknown; no automatic retry.|Bandposition unklar; keine Wiederholung.)'
    grep -Eq '(Rewind failed rc=-209 DOS=0|Zurueckspulen fehlgeschlagen rc=-209 DOS=0)' "$OUT/ui-native-uncertain.txt" || { printf 'quit\n'; return 1; }
    ui_key esc
    return 0
fi
ui_capture native-rewound '(Rewound; query status again.|Zurueckgespult; Status neu abfragen.)'
ui_key v
ui_capture native-verify-prompt '(Read NRMT0 from current position|NRMT0 ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture native-verified '(Archive verification OK; tape closed.|Archivpruefung OK; Band geschlossen.)'
ui_key r
ui_capture native-catalog-rewind-prompt '(Really rewind NRMT0|NRMT0 wirklich zurueckspulen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture native-catalog-rewound '(Rewound; query status again.|Zurueckgespult; Status neu abfragen.)'
ui_key l
ui_capture native-catalog-prompt '(Load NRMT0 contents from current position|NRMT0 Inhalt ab aktueller Position lesen)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture native-catalog 'DATA/INPUT.BIN'
ui_key spc
ui_key e
ui_capture native-export-prompt '(Export 1 marked files|1 markierte Dateien)'
if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
ui_capture native-exported '(Selection saved;|Auswahl gespeichert;)'
ui_key b
ui_capture native-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

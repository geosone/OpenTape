# No tape loaded, probed, enumerated or queried: exact zero CDB fixture.
source "$ROOT/tests/ui_monitor_helpers.sh"
if test "$variant" != ui-catalog-start; then
    ui_capture offline-start '(Devices and tape status|Geraete und Bandstatus)'
    ui_key f9
    ui_capture offline-edit '(Catalogue file:|Katalogdatei:)'
    ui_key f2
    ui_capture offline-confirm '(Perform this catalogue file operation now|Diesen Katalogvorgang jetzt ausfuehren)'
    if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
    ui_capture offline-loaded '(Offline metadata loaded|Offline-Metadaten geladen)'
    ui_key esc
fi
ui_capture offline-view '(Saved catalogue: OFFLINE|Gespeicherter Katalog: OFFLINE)'
if test "$variant" = ui-catfile-large; then
    grep -Eq '(Entries: 256|Eintraege: 256)' "$OUT/ui-offline-view.txt"
    ui_key end
    ui_capture offline-last '(shown: 251-256|Anzeige: 251-256)'
    for n in $(seq 1 29); do ui_key right; sleep 0.12; done
    ui_capture offline-tail 'F255.BIN'
else
    grep -Fq 'DATA/INPUT.BIN' "$OUT/ui-offline-view.txt"
fi
ui_key spc
ui_capture offline-mark '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)'
for key in o c w k; do
    ui_key "$key"
    ui_capture "offline-block-$key" '(Offline: first read live catalogue|Offline: erst mit L Katalog vom Band lesen)'
done
if test "$variant" != ui-catfile-large; then
    ui_key s
    ui_capture offline-search '(Catalogue search|Katalogsuche)'
    for key in i n p u t; do printf 'sendkey %s 30\n' "$key"; sleep 0.06; done
    ui_key f2
    ui_capture offline-found '(Match 1/1|Treffer 1/1)'
fi
# A cancelled replacement import preserves both offline provenance and marks.
ui_key f9
ui_capture offline-reedit '(Catalogue file:|Katalogdatei:)'
ui_key f2
ui_capture offline-reconfirm '(Perform this catalogue file operation now|Diesen Katalogvorgang jetzt ausfuehren)'
ui_key esc
ui_capture offline-cancelled '(Saved catalogue: OFFLINE|Gespeicherter Katalog: OFFLINE)'
grep -Eq '(Marked: 1 files; replace: 0|Auswahl: 1 Dateien; Ersetzen: 0)' "$OUT/ui-offline-cancelled.txt"
ui_key b
ui_capture offline-done '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

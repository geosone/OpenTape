# Confirmed read-only comparison. Every variant must preserve the entire disk.
ui_key k
if test "$variant" = ui-compare-disabled; then
    ui_capture compare-disabled-empty '(No target: --ui --compare-to|Ziel fehlt: --ui --compare-to)'
else
    ui_capture compare-empty '(Mark files first.|Zuerst Dateien markieren.)'
fi
ui_key esc
ui_capture compare-empty-back 'DATA/INPUT.BIN'
if test "$variant" = ui-compare-tree; then ui_key home; ui_key down; fi
ui_key spc
ui_key o
ui_capture compare-grant '(replace: 1|Ersetzen: 1)'
ui_key k
if test "$variant" = ui-compare-disabled; then
    ui_capture compare-disabled '(No target: --ui --compare-to|Ziel fehlt: --ui --compare-to)'
    ui_key esc
else
    ui_capture compare-confirm '(Compare 1 selected files with archive|1 Dateien mit dem Archiv vergleichen)'
    grep -Eq '(Read only; replacement grants are retained.|Nur lesen; Ersetzungsfreigaben bleiben.)' "$OUT/ui-compare-confirm.txt"
    grep -Eq '(Size/CRC/EOF|Groesse/CRC/EOF)' "$OUT/ui-compare-confirm.txt"
    # O cancels; it cannot also revoke the live replacement permission.
    ui_key o
    ui_capture compare-cancelled '(replace: 1|Ersetzen: 1)'
    if test "$variant" != ui-compare-cancel; then
        ui_key k
        ui_capture compare-confirm-again '(Compare 1 selected files with archive|1 Dateien mit dem Archiv vergleichen)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        case "$variant" in
            ui-compare|ui-compare-paths|ui-compare-legacy|ui-compare-fixed|ui-compare-tree)
                ui_capture compare-ok '(Compare OK; archive and files checked.|Vergleich OK; Archiv und Dateien geprueft.)'
                grep -Eq '(Files checked=1 rc=0|Dateien geprueft=1 rc=0)' "$OUT/ui-compare-ok.txt"
                if test "$variant" = ui-compare-tree; then
                    grep -Eq '(Matched=1 Kept=0 Excluded=1|Gleich=1 Behalten=0 Ausgeschlossen=1)' "$OUT/ui-compare-ok.txt"
                fi ;;
            ui-compare-mismatch|ui-compare-short)
                ui_capture compare-different '(File content or length differs.|Dateiinhalt oder Laenge unterscheidet sich.)'
                grep -Fq 'INPUT.BIN' "$OUT/ui-compare-different.txt"
                grep -Eq '(Compare FAILED|Vergleich FEHLER)' "$OUT/ui-compare-different.txt" ;;
            ui-compare-missing)
                ui_capture compare-file-error '(File could not be checked.|Datei konnte nicht geprueft werden.)'
                grep -Fq 'INPUT.BIN' "$OUT/ui-compare-file-error.txt" ;;
            *)
                ui_capture compare-incomplete '(Incomplete; counts are provisional.|Unvollstaendig; Zaehler nur vorlaeufig.)'
                grep -Eq '(Compare FAILED|Vergleich FEHLER)' "$OUT/ui-compare-incomplete.txt"
                if test "$variant" = ui-compare-late-fault; then
                    grep -Eq '(Files checked=1 rc=0|Dateien geprueft=1 rc=0)' "$OUT/ui-compare-incomplete.txt"
                fi ;;
        esac
        # A result consumes W; it cannot accidentally enter the write action.
        ui_key w
        ui_capture compare-result-back '(replace: 1|Ersetzen: 1)'
    fi
fi
ui_key b
ui_capture compare-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

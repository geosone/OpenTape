# Read-only preview and one explicitly permitted replacement on a memory tape.
# The host compares originals/outputs and exact CDB counts after DOS exits.
ui_key o
ui_capture replace-unmarked '(replace: 0|Ersetzen: 0)'
if test "$variant" = ui-replace-tree; then
    ui_key a
    ui_key home
    ui_key down
else
    ui_key spc
fi
ui_capture replace-selected '(Marked: [12] files|Auswahl: [12] Dateien)'

case "$variant" in
    ui-replace-rewind-fault|ui-replace-read-fault|ui-replace-late-fault|ui-replace-staging) ;;
    *)
        # Marks alone keep existing files; no replacement consent is inferred.
        ui_key c
        ui_capture replace-preview-keep '(Replacement grants for this batch: 0|Ersetzungsfreigaben fuer diesen Lauf: 0)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        ui_capture replace-preview-kept '(Preview OK; tape closed.|Vorschau OK; Band geschlossen.)'
        grep -Eq '(Planned replacements: 0|Geplante Ersetzungen: 0)' "$OUT/ui-replace-preview-kept.txt"
        grep -Eq '(New=0 Kept=[12] Excluded=0|Neu=0 Behalten=[12] Ausgeschlossen=0)' "$OUT/ui-replace-preview-kept.txt"
        ui_key esc
        ui_capture replace-before-grant '(replace: 0|Ersetzen: 0)'
        ;;
esac
ui_key o
ui_capture replace-granted '(replace: 1|Ersetzen: 1)'
grep -Fq '[!]' "$OUT/ui-replace-granted.txt"
# Clearing the extraction mark must also remove the permission. A selects the
# file again but does not recreate it. N clears both kinds of live intent.
ui_key spc
ui_capture replace-deselected '(replace: 0|Ersetzen: 0)'
ui_key a
ui_capture replace-select-all '(replace: 0|Ersetzen: 0)'
ui_key o
ui_capture replace-again '(replace: 1|Ersetzen: 1)'
ui_key n
ui_capture replace-clear '(Marked: 0 files; replace: 0|Auswahl: 0 Dateien; Ersetzen: 0)'
ui_key a
ui_key o
ui_capture replace-final-grant '(replace: 1|Ersetzen: 1)'
case "$variant" in
    ui-replace-rewind-fault|ui-replace-read-fault|ui-replace-late-fault|ui-replace-staging) ;;
    *)
        ui_key c
        ui_capture replace-preview-plan '(Replacement grants for this batch: 1|Ersetzungsfreigaben fuer diesen Lauf: 1)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        ui_capture replace-preview-planned '(Preview OK; tape closed.|Vorschau OK; Band geschlossen.)'
        grep -Eq '(Planned replacements: 1|Geplante Ersetzungen: 1)' "$OUT/ui-replace-preview-planned.txt"
        ui_key esc
        ui_capture replace-preview-retained '(replace: 1|Ersetzen: 1)'
        ;;
esac
ui_key w
if test "$variant" = ui-replace-disabled; then
    ui_capture replace-disabled '(No target: --ui --restore-to|Ziel fehlt: --ui --restore-to)'
    ui_key esc
    ui_capture replace-disabled-retained '(replace: 1|Ersetzen: 1)'
else
    ui_capture replace-write-confirm '(Replacement grants for this batch: 1|Ersetzungsfreigaben fuer diesen Lauf: 1)'
    grep -Eq '(Writes files; only \[!\] may replace.|Schreibt Dateien; nur \[!\] darf ersetzen.)' "$OUT/ui-replace-write-confirm.txt"
    ui_key esc
    ui_capture replace-write-cancel '(replace: 1|Ersetzen: 1)'
    if test "$variant" != ui-replace-cancel; then
        ui_key w
        ui_capture replace-write-again '(Replacement grants for this batch: 1|Ersetzungsfreigaben fuer diesen Lauf: 1)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        case "$variant" in
            ui-replace-rewind-fault|ui-replace-staging)
                ui_capture replace-stopped '(Stopped; no restore output created.|Gestoppt; keine Restore-Ausgabe angelegt.)'
                grep -Eq '(Files replaced: 0|Dateien ersetzt: 0)' "$OUT/ui-replace-stopped.txt" ;;
            ui-replace-read-fault)
                ui_capture replace-read-failed '(Incomplete; files/staging may remain.|Unvollstaendig; Dateien/TMP koennen bleiben.)'
                grep -Eq '(Files replaced: 0|Dateien ersetzt: 0)' "$OUT/ui-replace-read-failed.txt" ;;
            ui-replace-late-fault)
                ui_capture replace-late-failed '(Incomplete; files/staging may remain.|Unvollstaendig; Dateien/TMP koennen bleiben.)'
                grep -Eq '(Files replaced: 1|Dateien ersetzt: 1)' "$OUT/ui-replace-late-failed.txt" ;;
            *)
                ui_capture replace-written '(Restore OK; data checked, tape closed.|Restore OK; Daten geprueft, Band geschlossen.)'
                grep -Eq '(Files replaced: 1|Dateien ersetzt: 1)' "$OUT/ui-replace-written.txt"
                grep -Eq '(Readback=1 rc=0 Recovery=0|Ruecklesen=1 rc=0 Recovery=0)' "$OUT/ui-replace-written.txt"
                if test "$variant" = ui-replace-tree; then
                    grep -Eq '(Files=1 Kept=1 Excluded=0|Dateien=1 Behalten=1 Ausgeschlossen=0)' "$OUT/ui-replace-written.txt"
                fi ;;
        esac
        ui_key esc
        ui_capture replace-consumed '(replace: 0|Ersetzen: 0)'
        # A second confirmed W now has no grant and refuses the existing root.
        ui_key w
        ui_capture replace-no-grant '(Replacement grants for this batch: 0|Ersetzungsfreigaben fuer diesen Lauf: 0)'
        if test "$MAXLANG" = GR; then ui_key j; else ui_key y; fi
        ui_capture replace-no-replay '(Stopped; no restore output created.|Gestoppt; keine Restore-Ausgabe angelegt.)'
        ui_key esc
        ui_capture replace-no-replay-back '(replace: 0|Ersetzen: 0)'
    fi
fi
ui_key b
ui_capture replace-back '(Devices and tape status|Geraete und Bandstatus)'
ui_key esc

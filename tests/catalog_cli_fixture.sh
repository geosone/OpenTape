# Sourced during batch construction. Keep CLI-only cases out of the ordinary
# archive/restore sequence, so exact CDB totals isolate the requested action.
if [[ "$variant" = catalog-cli-* ]] || test "$variant" = ui-catalog-start; then
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    case "$variant" in
        catalog-cli-live|catalog-cli-legacy|catalog-cli-fixed|catalog-cli-corrupt)
            lines+=('OPENTAPE --native --backup-file INPUT.BIN --as DATA/INPUT.BIN --device NRMT0 --overwrite --volume TEST > AUX'
                'IF ERRORLEVEL 1 GOTO FAILED' 'OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
            command='OPENTAPE --native --list --catalog-out BATCH.CAT > AUX'
            if test "$variant" = catalog-cli-legacy; then command=${command/ --native/}; fi
            lines+=("$command")
            if test "$variant" = catalog-cli-corrupt; then
                lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
            else
                lines+=('IF ERRORLEVEL 1 GOTO FAILED' 'OPENTAPE --list --catalog-in BATCH.CAT > AUX'
                    'IF ERRORLEVEL 1 GOTO FAILED' "$command" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
            fi ;;
        catalog-cli-invalid)
            for arguments in '--list --catalog-in' '--list --catalog-out' '--ui --catalog-in' \
                '--list --catalog-in ONE.CAT --catalog-in TWO.CAT' \
                '--list --catalog-out ONE.CAT --catalog-out TWO.CAT' \
                '--ui --catalog-in ONE.CAT --catalog-in TWO.CAT' \
                '--list --catalog-in INPUT.CAT --device NRMT0' \
                '--list --catalog-in INPUT.CAT --catalog-out COPY.CAT' \
                '--list --catalog-out COPY.CAT --selection-out COPY.LST' \
                '--list --catalog-in INPUT.CAT --selection-out COPY.LST' \
                '--restore --catalog-in INPUT.CAT' '--verify --all --catalog-out COPY.CAT' \
                '--list --catalog-in RMT0.CAT' '--list --catalog-out RMT0.CAT' '--ui --catalog-in RMT0.CAT' \
                '--list --catalog-in NUL.CAT' '--list --catalog-out CON.CAT' \
                '--list --catalog-in BAD?.CAT' '--list --catalog-out BAD*.CAT' \
                '--ui --catalog-out COPY.CAT' \
                '--list --catalog-in ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345.CAT'; do
                lines+=("OPENTAPE $arguments > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
            done
            for arguments in '--list --catalog-in BROKEN.CAT' '--list --catalog-in NONE.CAT' \
                '--ui --catalog-in BROKEN.CAT' '--ui --catalog-in NONE.CAT'; do
                lines+=("OPENTAPE $arguments > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
            done ;;
        catalog-cli-staging)
            for target in BATCH.CAT OTHER.CAT; do
                lines+=("OPENTAPE --native --list --catalog-out $target > AUX"
                    'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
            done ;;
        catalog-cli-offline|catalog-cli-empty)
            lines+=('OPENTAPE --list --catalog-in OPENTAPE.CAT > AUX' 'IF ERRORLEVEL 1 GOTO FAILED') ;;
        catalog-cli-large)
            lines+=('CATIO > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
                'OPENTAPE --list --catalog-in OPENTAPE.CAT > AUX' 'IF ERRORLEVEL 1 GOTO FAILED') ;;
        ui-catalog-start)
            lines+=('OPENTAPE --ui --catalog-in OPENTAPE.CAT > AUX' 'IF ERRORLEVEL 1 GOTO FAILED') ;;
    esac
fi

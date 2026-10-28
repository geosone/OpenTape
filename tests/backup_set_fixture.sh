if [[ "$variant" = set-* ]] || [[ "$variant" = ui-set* ]]; then
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'BSETTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    command='OPENTAPE --native --backup-set SET.LST --as DATA --device NRMT0 --overwrite --volume TEST > AUX'
    case "$variant" in
        set-cli|set-cli-legacy|set-cli-fixed)
            if test "$variant" = set-cli-legacy; then command=${command/ --native/}; fi
            lines+=("$command" 'IF ERRORLEVEL 1 GOTO FAILED') ;;
        set-invalid)
            for arguments in '--backup-set' '--backup-set SET.LST --as DATA --device NRMT0 --volume TEST' \
                '--backup-set SET.LST --as DATA --device NRMT0 --overwrite --volume BAD!' \
                '--ui --backup-set SET.LST --backup-file INPUT.BIN --as DATA --volume TEST'; do
                lines+=("OPENTAPE $arguments > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
            done
            for manifest in BAD.LST EMPTY.LST DUP.LST PREFIX.LST ALIAS.LST CANON.LST MISSING.LST; do
                lines+=("${command/SET.LST/$manifest}" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
            done ;;
        set-plan-limit)
            lines+=("${command/SET.LST/LIMIT.LST}" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED') ;;
        ui-set*)
            lines+=('OPENTAPE --native --ui > AUX' 'IF ERRORLEVEL 1 GOTO FAILED') ;;
    esac
    if test "$variant" = set-cli || test "$variant" = set-cli-legacy || test "$variant" = set-cli-fixed || { [[ "$variant" = ui-set* ]] && test "$variant" != ui-set-depth-limit; }; then
        lines+=('OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --compare --device NRMT0 --to CHECK > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
fi

#!/bin/bash
# Real archive CLI roundtrip against a resident, memory-only ASPI medium.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-archive-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all >/dev/null
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/PREVTEST.EXE" "$OUT/bin-$MAXLANG/STRMTEST.EXE" "$OUT/bin-$MAXLANG/BUPTEST.EXE" "$OUT/bin-$MAXLANG/MOUSTEST.EXE" "$OUT/bin-$MAXLANG/FORMTEST.EXE" "$OUT/bin-$MAXLANG/PATHTEST.EXE" "$OUT/bin-$MAXLANG/FINDTEST.EXE" "$OUT/bin-$MAXLANG/CSELTEST.EXE" "$OUT/bin-$MAXLANG/SETTEST.EXE" "$OUT/bin-$MAXLANG/PSUMTEST.EXE" "$OUT/bin-$MAXLANG/PRGTEST.EXE" "$OUT/bin-$MAXLANG/RESTEST.EXE" "$OUT/bin-$MAXLANG/CATTEST.EXE" "$OUT/bin-$MAXLANG/CATIO.EXE" "$OUT/bin-$MAXLANG/BSETTEST.EXE" "$OUT/bin-$MAXLANG/VPRTEST.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal tree replace legacy combined eom eod corrupt read-hard write-short flush mark end ui ui-failed ui-legacy ui-unfinished)
variants+=(ui-restore ui-restore-legacy ui-restore-fixed ui-restore-tree ui-restore-cancel ui-restore-disabled
    ui-restore-existing ui-restore-invalid ui-restore-rewind-fault ui-restore-read-fault ui-restore-late-fault)
variants+=(ui-replace ui-replace-legacy ui-replace-fixed ui-replace-tree ui-replace-cancel
    ui-replace-disabled ui-replace-rewind-fault ui-replace-read-fault ui-replace-late-fault ui-replace-staging)
variants+=(ui-compare ui-compare-legacy ui-compare-fixed ui-compare-tree ui-compare-mismatch
    ui-compare-short ui-compare-missing ui-compare-root ui-compare-invalid ui-compare-staging
    ui-compare-cancel ui-compare-disabled ui-compare-rewind-fault ui-compare-read-fault ui-compare-late-fault)
variants+=(ui-import ui-import-legacy ui-import-fixed ui-import-restore ui-import-unknown ui-import-duplicate
    ui-import-empty ui-import-bad ui-import-directory ui-import-missing ui-import-limit ui-import-cancel ui-import-disabled)
variants+=(ui-backup-depth ui-backup-depth-limit ui-backup ui-backup-tree ui-backup-fixed ui-backup-legacy ui-backup-disabled ui-backup-cancel
    ui-backup-missing ui-backup-bad-tree ui-backup-device ui-backup-short ui-backup-flush ui-backup-mark ui-backup-end backup-service)
variants+=(ui-backup-form ui-backup-form-tree-mouse ui-backup-form-cancel ui-backup-form-invalid ui-backup-form-limit-mouse)
variants+=(ui-restore-progress ui-restore-progress-mouse ui-preview-progress ui-compare-progress ui-restore-progress-late ui-restore-progress-tree)
variants+=(ui-backup-progress ui-backup-progress-tree-mouse ui-backup-progress-flush)
variants+=(ui-preflight-file ui-preflight-tree-mouse ui-preflight-missing ui-preflight-alias ui-preflight-tree-alias ui-preflight-depth ui-preflight-limit ui-preflight-disabled)
variants+=(ui-verify-progress ui-verify-progress-mouse ui-verify-progress-legacy ui-verify-progress-fixed ui-verify-progress-tree ui-verify-progress-late)
variants+=(set-cli set-cli-legacy set-cli-fixed set-invalid set-plan-limit ui-set ui-set-mouse ui-set-progress ui-set-depth ui-set-depth-limit)
variants+=(catalog-cli-live catalog-cli-legacy catalog-cli-fixed catalog-cli-offline catalog-cli-empty catalog-cli-invalid catalog-cli-staging catalog-cli-corrupt catalog-cli-large ui-catalog-start)
variants+=(ui-catfile-large ui-catfile-offline ui-catfile ui-catfile-mouse ui-catfile-corrupt ui-catfile-staging ui-catfile-missing ui-catfile-alias)
variants+=(ui-profile ui-profile-mouse ui-profile-missing ui-profile-corrupt ui-profile-staging ui-profile-empty ui-profile-device)
variants+=(ui-directory ui-directory-mouse ui-restore-directory ui-restore-directory-mouse)
variants+=(ui-search ui-search-mouse)
variants+=(ui-preview-paths ui-compare-paths ui-restore-paths ui-import-paths-mouse ui-paths-cancel ui-paths-disable)
variants+=(import-service compare-service replace-service restore-service preview-service ui-preview-keep ui-preview-invalid ui-preview-disabled ui-preview-rewind-fault ui-preview-read-fault)
variants+=(fixed fixed-legacy fixed-combined fixed-corrupt fixed-padding fixed-hard fixed-eom fixed-eod
    fixed-short fixed-flush fixed-mark fixed-end fixed-ui)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_ARCHIVE_TEST)
    backup_exit=0 read_exit=0 calls=64 backup_kind=--backup-file backup_source=INPUT.BIN ui_mode=0 fixed_unit=0
    case "$variant" in
        normal) ;;
        ui-backup-form) ui_mode=1; calls=19 ;;
        ui-backup-form-tree-mouse) ui_mode=1; calls=27; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-form-cancel|ui-backup-form-invalid|ui-backup-form-limit-mouse) ui_mode=1; calls=0 ;;
        ui-preview-paths|ui-compare-paths|ui-restore-paths) ui_mode=1; calls=37 ;;
        ui-verify-progress|ui-verify-progress-mouse|ui-verify-progress-legacy) ui_mode=1; calls=28; defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_READ_PROGRESS_START=1 -DARCHIVE_UI_FAULT=0) ;;
        ui-verify-progress-fixed) ui_mode=1; calls=53; fixed_unit=512; defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_READ_PROGRESS_START=1 -DARCHIVE_UI_FAULT=0) ;;
        ui-verify-progress-tree) ui_mode=1; calls=40; backup_kind=--backup-tree; backup_source=SRC; defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_READ_PROGRESS_START=1 -DARCHIVE_UI_FAULT=0) ;;
        ui-verify-progress-late) ui_mode=1; calls=28; defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_READ_PROGRESS_START=1 -DARCHIVE_UI_FAULT=3) ;;
        ui-restore-progress|ui-restore-progress-mouse|ui-preview-progress|ui-compare-progress) defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_UI_FAULT=0); ui_mode=1; calls=37 ;;
        ui-restore-progress-late) defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_UI_FAULT=3); ui_mode=1; calls=36 ;;
        ui-restore-progress-tree) defines+=(-DARCHIVE_READ_PROGRESS_PAUSE -DARCHIVE_UI_FAULT=0); ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-progress) defines+=(-DARCHIVE_PROGRESS_PAUSE); ui_mode=1; calls=19 ;;
        ui-backup-progress-tree-mouse) defines+=(-DARCHIVE_PROGRESS_PAUSE); ui_mode=1; calls=27; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-progress-flush) defines+=(-DARCHIVE_PROGRESS_PAUSE -DNATIVE_FINISH_FAULT=1); ui_mode=1; calls=8 ;;
        ui-preflight-tree-mouse|ui-preflight-tree-alias|ui-preflight-depth|ui-preflight-limit) ui_mode=1; calls=0; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-preflight*) ui_mode=1; calls=0 ;;
        ui-profile*) ui_mode=1; calls=0 ;;
        set-cli) ui_mode=9; calls=29 ;;
        set-cli-legacy) ui_mode=9; calls=28; defines+=(-DARCHIVE_LEGACY_FINISH) ;;
        set-cli-fixed) ui_mode=9; fixed_unit=512; calls=45 ;;
        set-invalid|set-plan-limit) ui_mode=9; calls=0 ;;
        ui-set|ui-set-mouse) ui_mode=1; calls=29 ;;
        ui-set-progress) ui_mode=1; calls=29; defines+=(-DARCHIVE_PROGRESS_PAUSE) ;;
        ui-set-depth) ui_mode=1; calls=53 ;;
        ui-set-depth-limit) ui_mode=1; calls=0 ;;
        catalog-cli-live|catalog-cli-legacy) ui_mode=9; calls=19 ;;
        catalog-cli-fixed) ui_mode=9; calls=35; fixed_unit=512 ;;
        catalog-cli-corrupt) ui_mode=9; calls=12; defines+=(-DARCHIVE_CORRUPT) ;;
        catalog-cli-offline|catalog-cli-empty|catalog-cli-invalid|catalog-cli-staging|catalog-cli-large) ui_mode=9; calls=0 ;;
        ui-catalog-start) ui_mode=1; calls=0 ;;
        ui-catfile-offline|ui-catfile-large) ui_mode=1; calls=0 ;;
        ui-catfile*) ui_mode=1; calls=28 ;;
        ui-directory|ui-directory-mouse) ui_mode=1; calls=43; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-restore-directory|ui-restore-directory-mouse) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-search|ui-search-mouse) ui_mode=1; calls=40; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-paths-cancel) ui_mode=1; calls=10 ;;
        ui-paths-disable) ui_mode=1; calls=28 ;;
        ui-import-paths-mouse) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        backup-service) ui_mode=7; calls=19 ;;
        ui-backup|ui-backup-mouse) ui_mode=1; calls=19 ;;
        ui-backup-legacy) defines+=(-DARCHIVE_LEGACY_FINISH); ui_mode=1; calls=18 ;;
        ui-backup-depth) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-depth-limit) ui_mode=1; calls=0; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-tree) ui_mode=1; calls=27; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-backup-fixed) fixed_unit=512; ui_mode=1; calls=35 ;;
        ui-backup-disabled|ui-backup-cancel|ui-backup-missing|ui-backup-bad-tree|ui-backup-device) ui_mode=1; calls=0 ;;
        ui-backup-short) defines+=(-DARCHIVE_WRITE_SHORT); ui_mode=1; calls=1 ;;
        ui-backup-flush) defines+=(-DNATIVE_FINISH_FAULT=1); ui_mode=1; calls=8 ;;
        ui-backup-mark) defines+=(-DNATIVE_FINISH_FAULT=2); ui_mode=1; calls=9 ;;
        ui-backup-end) defines+=(-DNATIVE_FINISH_FAULT=3); ui_mode=1; calls=10 ;;
        setmark) defines+=(-DARCHIVE_SETMARK); read_exit=2; calls=19 ;;
        combined-setmark) defines+=(-DARCHIVE_SETMARK -DARCHIVE_COMBINED_MARK); read_exit=2; calls=18 ;;
        fixed-setmark) fixed_unit=512; defines+=(-DARCHIVE_SETMARK); read_exit=2; calls=35 ;;
        fixed-combined-setmark) fixed_unit=512; defines+=(-DARCHIVE_SETMARK -DARCHIVE_COMBINED_MARK); read_exit=2; calls=34 ;;
        fixed) fixed_unit=512; calls=125 ;;
        fixed-ui) fixed_unit=512; ui_mode=1; calls=71 ;;
        fixed-legacy) fixed_unit=512; defines+=(-DNO_RESIDUAL); calls=125 ;;
        fixed-combined) fixed_unit=512; defines+=(-DARCHIVE_COMBINED_MARK); calls=119 ;;
        fixed-corrupt) fixed_unit=512; defines+=(-DARCHIVE_CORRUPT); read_exit=2; calls=19 ;;
        fixed-padding) fixed_unit=512; defines+=(-DARCHIVE_PADDING_BAD); read_exit=2; calls=19 ;;
        fixed-hard) fixed_unit=512; defines+=(-DARCHIVE_READ_HARD); read_exit=2; calls=19 ;;
        fixed-eom) fixed_unit=512; defines+=(-DARCHIVE_EOM); read_exit=2; calls=35 ;;
        fixed-eod) fixed_unit=512; defines+=(-DARCHIVE_EOD); read_exit=2; calls=35 ;;
        fixed-short) fixed_unit=512; defines+=(-DARCHIVE_WRITE_SHORT); backup_exit=2; calls=1 ;;
        fixed-flush) fixed_unit=512; defines+=(-DNATIVE_FINISH_FAULT=1); backup_exit=2; calls=15 ;;
        fixed-mark) fixed_unit=512; defines+=(-DNATIVE_FINISH_FAULT=2); backup_exit=2; calls=16 ;;
        fixed-end) fixed_unit=512; defines+=(-DNATIVE_FINISH_FAULT=3); backup_exit=2; calls=17 ;;
        ui-mouse) ui_mode=1; calls=28 ;;
        ui) ui_mode=1; calls=37 ;;
        ui-legacy) ui_mode=1; calls=37 ;;
        ui-import-fixed) fixed_unit=512; ui_mode=1; calls=65; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-import-restore|ui-import-mouse) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-import*) ui_mode=1; calls=40; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-compare|ui-compare-legacy) ui_mode=1; calls=37 ;;
        ui-compare-fixed) fixed_unit=512; ui_mode=1; calls=71 ;;
        ui-compare-tree) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-compare-mismatch|ui-compare-short|ui-compare-missing) ui_mode=1; calls=35 ;;
        ui-compare-root|ui-compare-invalid|ui-compare-staging|ui-compare-cancel|ui-compare-disabled) ui_mode=1; calls=28 ;;
        ui-compare-rewind-fault) defines+=(-DARCHIVE_UI_FAULT=1); ui_mode=1; calls=29 ;;
        ui-compare-read-fault) defines+=(-DARCHIVE_UI_FAULT=2); ui_mode=1; calls=30 ;;
        ui-compare-late-fault) defines+=(-DARCHIVE_UI_FAULT=3); ui_mode=1; calls=36 ;;
        ui-replace|ui-replace-legacy) ui_mode=1; calls=55 ;;
        ui-replace-fixed) fixed_unit=512; ui_mode=1; calls=107 ;;
        ui-replace-tree) ui_mode=1; calls=79; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-replace-cancel|ui-replace-disabled) ui_mode=1; calls=46 ;;
        ui-replace-rewind-fault) defines+=(-DARCHIVE_UI_FAULT=1); ui_mode=1; calls=29 ;;
        ui-replace-read-fault) defines+=(-DARCHIVE_UI_FAULT=2); ui_mode=1; calls=30 ;;
        ui-replace-late-fault) defines+=(-DARCHIVE_UI_FAULT=3); ui_mode=1; calls=36 ;;
        ui-replace-staging) ui_mode=1; calls=28 ;;
        ui-restore) ui_mode=1; calls=37 ;;
        ui-restore-legacy) ui_mode=1; calls=37 ;;
        ui-restore-fixed) fixed_unit=512; ui_mode=1; calls=71 ;;
        ui-restore-tree) ui_mode=1; calls=53; backup_kind=--backup-tree; backup_source=SRC ;;
        ui-restore-cancel|ui-restore-disabled|ui-restore-existing|ui-restore-invalid) ui_mode=1; calls=28 ;;
        ui-restore-rewind-fault) defines+=(-DARCHIVE_UI_FAULT=1); ui_mode=1; calls=29 ;;
        ui-restore-read-fault) defines+=(-DARCHIVE_UI_FAULT=2); ui_mode=1; calls=30 ;;
        ui-restore-late-fault) defines+=(-DARCHIVE_UI_FAULT=3); ui_mode=1; calls=36 ;;
        import-service) ui_mode=6; calls=10 ;;
        compare-service) ui_mode=5; calls=28 ;;
        replace-service) ui_mode=4; calls=37 ;;
        restore-service) ui_mode=3; calls=28 ;;
        preview-service) ui_mode=2; calls=28 ;;
        ui-preview-rewind-fault) defines+=(-DARCHIVE_UI_FAULT=1); ui_mode=1; calls=29 ;;
        ui-preview-read-fault) defines+=(-DARCHIVE_UI_FAULT=2); ui_mode=1; calls=30 ;;
        ui-preview-keep) ui_mode=1; calls=37 ;;
        ui-preview-invalid) ui_mode=1; calls=28 ;;
        ui-preview-disabled) ui_mode=1; calls=28 ;;
        ui-unfinished) defines+=(-DNATIVE_FINISH_FAULT=3); ui_mode=1; backup_exit=2; calls=11 ;;
        ui-failed) defines+=(-DNATIVE_CONTROL_FAULT=2); ui_mode=1; calls=11 ;;
        tree) calls=92; backup_kind=--backup-tree; backup_source=SRC ;;
        replace) calls=82 ;;
        legacy) defines+=(-DNO_RESIDUAL) ;;
        combined) defines+=(-DARCHIVE_COMBINED_MARK); calls=58 ;;
        eom) defines+=(-DARCHIVE_EOM); read_exit=2; calls=19 ;;
        eod) defines+=(-DARCHIVE_EOD); read_exit=2; calls=19 ;;
        corrupt) defines+=(-DARCHIVE_CORRUPT); read_exit=2; calls=12 ;;
        read-hard) defines+=(-DARCHIVE_READ_HARD); read_exit=2; calls=12 ;;
        write-short) defines+=(-DARCHIVE_WRITE_SHORT); backup_exit=2; calls=1 ;;
        flush) defines+=(-DNATIVE_FINISH_FAULT=1); backup_exit=2; calls=8 ;;
        mark) defines+=(-DNATIVE_FINISH_FAULT=2); backup_exit=2; calls=9 ;;
        end) defines+=(-DNATIVE_FINISH_FAULT=3); backup_exit=2; calls=10 ;;
        *) echo "Unknown archive fixture: $variant" >&2; exit 1 ;;
    esac
    if test "$fixed_unit" != 0; then defines+=("-DNATIVE_ARCHIVE_FIXED=$fixed_unit"); fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    python3 - "$run_dir/INPUT.BIN" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_bytes(bytes((i * 37 + 17) % 256 for i in range(35000)))
PY
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' 'DEVICE=ASPITAP.SYS -w' 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    archive_path=DATA/INPUT.BIN
    if test "$variant" = tree || test "$variant" = ui-verify-progress-tree || test "$variant" = ui-restore-tree || test "$variant" = ui-restore-progress-tree || test "$variant" = ui-replace-tree || test "$variant" = ui-compare-tree || [[ "$variant" = ui-search* ]] || [[ "$variant" = ui-directory* ]] || [[ "$variant" = ui-restore-directory* ]] || [[ "$variant" = ui-import* ]] || { test "$variant" = ui-backup-tree || test "$variant" = ui-backup-form-tree-mouse || test "$variant" = ui-backup-progress-tree-mouse; } || [[ "$variant" = ui-backup-depth* ]]; then archive_path=DATA; fi
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'OPENTAPE --native --status > AUX' 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED'
        "OPENTAPE --native $backup_kind $backup_source --as $archive_path --device NRMT3 --overwrite --volume TEST > AUX"
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
        "OPENTAPE --native $backup_kind $backup_source --as $archive_path --device NRMT0 --overwrite --volume TEST > AUX"
        "IF ERRORLEVEL $((backup_exit+1)) GOTO FAILED")
    if test "$ui_mode" = 2 || test "$ui_mode" = 3 || test "$ui_mode" = 4 || test "$ui_mode" = 5 || test "$ui_mode" = 6; then
        lines+=('IF ERRORLEVEL 1 GOTO FAILED')
        for arguments in '--ui --preview-to' '--ui --preview-to ONE --preview-to TWO' \
            '--ui --preview-to ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345' \
            '--ui --restore-to' '--ui --restore-to ONE --restore-to TWO' \
            '--ui --restore-to ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345' \
            '--ui --compare-to' '--ui --compare-to ONE --compare-to TWO' \
            '--ui --compare-to ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345' \
            '--ui --selection-in' '--ui --selection-in ONE --selection-in TWO' \
            '--ui --selection-in ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345'; do
            lines+=("OPENTAPE $arguments > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
        done
        if test "$ui_mode" = 6; then lines+=('STRMTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'PREVTEST --import > AUX')
        elif test "$ui_mode" = 5; then lines+=('PREVTEST --compare > AUX')
        elif test "$ui_mode" = 4; then lines+=('STRMTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'PREVTEST --replace > AUX')
        elif test "$ui_mode" = 3; then lines+=('PREVTEST --restore > AUX')
        else lines+=('PREVTEST > AUX'); fi
        lines+=('IF ERRORLEVEL 1 GOTO FAILED')
    elif test "$ui_mode" = 1; then
        if test "$backup_exit" = 2; then lines+=('IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
        ui_command='OPENTAPE --native --ui --selection-out UI.LST --preview-to PREVIEW > AUX'
        if test "$variant" = ui-verify-progress-legacy; then ui_command='OPENTAPE --ui > AUX'; fi
        if test "$variant" = ui-legacy; then ui_command='OPENTAPE --ui --selection-out UI.LST --preview-to PREVIEW > AUX'; fi
        if test "$variant" = ui-preview-invalid; then
            ui_command=${ui_command/PREVIEW/INPUT.BIN}
        elif test "$variant" = ui-preview-disabled; then
            ui_command=${ui_command/ --preview-to PREVIEW/}
        fi
        if [[ "$variant" = ui-restore* ]]; then
            ui_command='OPENTAPE --native --ui --restore-to RECOVER > AUX'
            if test "$variant" = ui-restore-legacy; then ui_command='OPENTAPE --ui --restore-to RECOVER > AUX'; fi
            if test "$variant" = ui-restore-disabled; then ui_command='OPENTAPE --native --ui > AUX'; fi
            if test "$variant" = ui-restore-invalid; then ui_command=${ui_command/RECOVER/INPUT.BIN}; fi
        fi
        if [[ "$variant" = ui-replace* ]]; then
            ui_command='OPENTAPE --native --ui --preview-to RECOVER --restore-to RECOVER > AUX'
            if test "$variant" = ui-replace-legacy; then ui_command=${ui_command/ --native/}; fi
            if test "$variant" = ui-replace-disabled; then ui_command=${ui_command/ --restore-to RECOVER/}; fi
        fi
        if [[ "$variant" = ui-compare* ]]; then
            ui_command='OPENTAPE --native --ui --compare-to RECOVER --restore-to RECOVER > AUX'
            if test "$variant" = ui-compare-legacy; then ui_command=${ui_command/ --native/}; fi
            if test "$variant" = ui-compare-disabled; then ui_command=${ui_command/ --compare-to RECOVER/}; fi
            if test "$variant" = ui-compare-root; then ui_command=${ui_command/--compare-to RECOVER/--compare-to MISSING}; fi
            if test "$variant" = ui-compare-invalid; then ui_command=${ui_command/--compare-to RECOVER/--compare-to INPUT.BIN}; fi
        fi
        if [[ "$variant" = ui-import* ]]; then
            ui_command='OPENTAPE --native --ui --selection-in INPUT.LST > AUX'
            if test "$variant" = ui-import-legacy; then ui_command=${ui_command/ --native/}; fi
            if test "$variant" = ui-import-disabled; then ui_command='OPENTAPE --native --ui > AUX'; fi
            if test "$variant" = ui-import-missing; then ui_command=${ui_command/INPUT.LST/NONE.LST}; fi
            if test "$variant" = ui-import-restore || test "$variant" = ui-import-mouse; then ui_command=${ui_command/ > AUX/ --restore-to RECOVER > AUX}; fi
        fi
        if test "$variant" = ui-mouse; then ui_command='OPENTAPE --native --ui --preview-to PREVIEW > AUX'; fi
        if [[ "$variant" = *paths* ]]; then ui_command='OPENTAPE --native --ui > AUX'; fi
        if test "$variant" = ui-paths-cancel; then
            ui_command='OPENTAPE --native --ui --preview-to PREVIEW --restore-to RECOVER --compare-to CHECK --selection-in INPUT.LST --selection-out UI.LST > AUX'
        fi
        lines+=("$ui_command" 'IF ERRORLEVEL 1 GOTO FAILED')
    elif test "$backup_exit" = 2; then
        lines+=('IF NOT ERRORLEVEL 2 GOTO FAILED' 'OPENTAPE --native-status > AUX'
            'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    else
        lines+=('IF ERRORLEVEL 1 GOTO FAILED' 'OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --verify --all > AUX' "IF ERRORLEVEL $((read_exit+1)) GOTO FAILED")
        if test "$read_exit" = 2; then
            lines+=('IF NOT ERRORLEVEL 2 GOTO FAILED')
        else
            lines+=('IF ERRORLEVEL 1 GOTO FAILED')
            for command in \
                'OPENTAPE --native --list --selection-out NATIVE.LST > AUX' \
                'OPENTAPE --native --scan > AUX' \
                'OPENTAPE --native --restore --device NRMT0 --to OUT --dry-run > AUX' \
                'OPENTAPE --native --restore --device NRMT0 --to OUT > AUX' \
                'OPENTAPE --native --compare --device NRMT0 --to OUT > AUX'; do
                lines+=('OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
                    "$command" 'IF ERRORLEVEL 1 GOTO FAILED')
            done
        fi
    fi
    if test "$variant" = replace; then
        lines+=('ECHO old > OUT\DATA\INPUT.BIN' 'OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --restore --device NRMT0 --to OUT --replace DATA/INPUT.BIN > AUX'
            'IF ERRORLEVEL 1 GOTO FAILED' 'OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --compare --device NRMT0 --to OUT > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    if [[ "$variant" = ui-backup* ]] || test "$variant" = backup-service; then
        # Start with an empty memory medium. The GUI/API is the only writer.
        lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
        for arguments in '--ui --backup-file INPUT.BIN' '--ui --as DATA' \
            '--ui --backup-file INPUT.BIN --as DATA --volume BAD!' \
            '--ui --backup-file INPUT.BIN --backup-tree SRC --as DATA --volume TEST'; do
            lines+=("OPENTAPE $arguments > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
        done
        if test "$variant" = backup-service; then lines+=('BUPTEST > AUX')
        else
            command="OPENTAPE --native --ui $backup_kind $backup_source --as $archive_path --volume TEST > AUX"
            if test "$variant" = ui-backup-legacy; then command=${command/ --native/}; fi
            if [[ "$variant" = ui-backup-form* ]] && test "$variant" != ui-backup-form-cancel; then command='OPENTAPE --native --ui > AUX'; fi
            if test "$variant" = ui-backup-disabled; then command='OPENTAPE --native --ui > AUX'; fi
            if test "$variant" = ui-backup-missing; then command=${command/INPUT.BIN/NONE.BIN}; fi
            if test "$variant" = ui-backup-bad-tree; then command=${command/--backup-file/--backup-tree}; fi
            if test "$variant" = ui-backup-device; then command=${command/ --ui/ --ui --device NRMT3}; fi
            lines+=("$command")
        fi
        lines+=('IF ERRORLEVEL 1 GOTO FAILED')
        case "$variant" in
            backup-service|ui-backup-progress|ui-backup-progress-tree-mouse|ui-backup-form|ui-backup-form-tree-mouse|ui-backup|ui-backup-mouse|ui-backup-tree|ui-backup-depth|ui-backup-fixed|ui-backup-legacy)
                lines+=('OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
                    'OPENTAPE --native --compare --device NRMT0 --to CHECK > AUX' 'IF ERRORLEVEL 1 GOTO FAILED') ;;
        esac
    fi
    if [[ "$variant" = ui-preflight* ]]; then
        command="OPENTAPE --native --ui $backup_kind $backup_source --as DATA --volume TEST > AUX"
        if test "$variant" = ui-preflight-disabled; then command='OPENTAPE --native --ui > AUX'; fi
        if test "$variant" = ui-preflight-missing; then command=${command/INPUT.BIN/NONE.BIN}; fi
        if test "$variant" = ui-preflight-alias; then command=${command/INPUT.BIN/RMT0.CFG}; fi
        lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'PSUMTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "$command" 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    if [[ "$variant" = ui-profile* ]]; then
        # No tape probe/seed: profile UI/codec must stay entirely offline.
        lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'SETTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --ui > AUX'
            'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    if test "$variant" = ui-catfile-offline || test "$variant" = ui-catfile-large; then
        lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native --ui > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    if test "$variant" = ui-catfile-large; then
        lines=("${lines[@]:0:3}" 'CATIO > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    source "$ROOT/tests/catalog_cli_fixture.sh"
    source "$ROOT/tests/backup_set_fixture.sh"
    lines+=('COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE ARCHIVE CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE ARCHIVE CLI FAILED > AUX' ':DONE' 'QEXIT')
    if test "$fixed_unit" != 0; then
        for index in "${!lines[@]}"; do
            lines[$index]=${lines[$index]/OPENTAPE --native /OPENTAPE --native --fixed-block $fixed_unit }
        done
        # Invalid mode/size syntax must return before acquiring or writing tape.
        rejected=()
        for size in 0 4097 9999999999999 nope +512; do
            rejected+=("OPENTAPE --native --fixed-block $size --verify --all > AUX"
                'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
        done
        lines=("${lines[@]:0:3}" "${rejected[@]}" "${lines[@]:3}")
    fi
    if [[ "$variant" = *mouse* ]]; then
        mouse_zip=${CTMOUSE_ZIP:-$HOME/Iso/Dos/cutemouse21b4.zip}
        unzip -p "$mouse_zip" bin/ctmouse.exe > "$run_dir/CTMOUSE.EXE"
        lines=("${lines[@]:0:3}" 'MOUSTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'CTMOUSE /P /R00 > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-backup-form* ]]; then
        lines=("${lines[@]:0:3}" 'FORMTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = *paths* ]]; then
        lines=("${lines[@]:0:3}" 'PATHTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-search* ]]; then
        lines=("${lines[@]:0:3}" 'FINDTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-directory* ]] || [[ "$variant" = ui-restore-directory* ]]; then
        lines=("${lines[@]:0:3}" 'CSELTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-backup-progress* ]]; then
        lines=("${lines[@]:0:3}" 'PRGTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-restore-progress* ]] || test "$variant" = ui-preview-progress || test "$variant" = ui-compare-progress; then
        lines=("${lines[@]:0:3}" 'RESTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-catfile* ]]; then
        lines=("${lines[@]:0:3}" 'CATTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    if [[ "$variant" = ui-verify-progress* ]]; then
        lines=("${lines[@]:0:3}" 'STRMTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'VPRTEST > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "${lines[@]:3}")
    fi
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" "$run_dir/MOCKASPI.SYS" \
        "$OPENASPI/build/dos/ASPITAP.SYS" "$OUT/bin-$MAXLANG/OPENTAPE.EXE" \
        "$OUT/bin-$MAXLANG/BUPTEST.EXE" "$OUT/bin-$MAXLANG/STRMTEST.EXE" "$OUT/bin-$MAXLANG/PREVTEST.EXE" "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" "$run_dir/INPUT.BIN" ::/
    if [[ "$variant" = *mouse* ]]; then
        mcopy -i "$run_dir/boot.img" "$run_dir/CTMOUSE.EXE" "$OUT/bin-$MAXLANG/MOUSTEST.EXE" ::/
    fi
    if [[ "$variant" = ui-verify-progress* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/VPRTEST.EXE" ::/
    fi
    if [[ "$variant" = ui-backup-form* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/FORMTEST.EXE" ::/
    fi
    if [[ "$variant" = *paths* ]]; then mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/PATHTEST.EXE" ::/; fi
    if [[ "$variant" = ui-search* ]]; then mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/FINDTEST.EXE" ::/; fi
    if [[ "$variant" = ui-directory* ]] || [[ "$variant" = ui-restore-directory* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/CSELTEST.EXE" ::/
    fi
    if [[ "$variant" = ui-catfile* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/CATTEST.EXE" "$OUT/bin-$MAXLANG/CATIO.EXE" ::/
        if test "$variant" = ui-catfile-offline; then
            python3 "$ROOT/tests/make_catalog_fixture.py" "$run_dir/OFFLINE.CAT"
            mcopy -i "$run_dir/boot.img" "$run_dir/OFFLINE.CAT" ::/OPENTAPE.CAT
        fi
        if test "$variant" = ui-catfile-corrupt; then
            printf 'not a catalogue' > "$run_dir/BROKEN.CAT"
            mcopy -i "$run_dir/boot.img" "$run_dir/BROKEN.CAT" ::/
        fi
        if test "$variant" = ui-catfile-staging; then
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" '::/OT$LIST.TMP'
        fi
        if test "$variant" = ui-catfile-alias; then
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" '::/RMT0.CAT'
        fi
    fi
    if [[ "$variant" = ui-profile* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/SETTEST.EXE" ::/
        python3 - "$run_dir" "$variant" <<'PY_PROFILE'
import pathlib, struct, sys, zlib
root, variant = pathlib.Path(sys.argv[1]), sys.argv[2]
def encode(empty=False):
    values = ['', 'DATA', 'BACKUP'] + [''] * 5 if empty else [
        'INPUT.BIN', 'DATA/INPUT.BIN', 'TEST', 'PREVIEW', 'RECOVER', 'CHECK', 'INPUT.LST', 'UI.LST']
    payload = bytes([0 if empty else 1, 0]) + b''.join(
        value.encode().ljust(size, b'\0') for value, size in zip(values, [41,41,32,41,41,41,41,41]))
    header = struct.pack('<4sHHI', b'OTCF', 1, 337, zlib.crc32(payload))
    return header + struct.pack('<I', zlib.crc32(header)) + payload
(root / 'EXPECTED.CFG').write_bytes(encode())
(root / 'EMPTY.CFG').write_bytes(encode(True))
if variant == 'ui-profile-corrupt':
    data = bytearray(encode()); data[-1] ^= 1; (root / 'BROKEN.CFG').write_bytes(data)
if variant == 'ui-profile-staging': (root / 'STAGE.BIN').write_bytes(b'preserve existing staging file')
PY_PROFILE
        mcopy -i "$run_dir/boot.img" "$run_dir/EMPTY.CFG" ::/
        if test "$variant" = ui-profile-device; then mcopy -i "$run_dir/boot.img" "$run_dir/EXPECTED.CFG" ::/RMT0.CFG; fi
        if test "$variant" = ui-profile-corrupt; then mcopy -i "$run_dir/boot.img" "$run_dir/BROKEN.CFG" ::/OPENTAPE.CFG; fi
        if test "$variant" = ui-profile-staging; then mcopy -i "$run_dir/boot.img" "$run_dir/STAGE.BIN" '::/OT$LIST.TMP'; fi
    fi
    if [[ "$variant" = ui-preflight* ]]; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/PSUMTEST.EXE" ::/
        if test "$variant" = ui-preflight-alias; then mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" ::/RMT0.CFG; fi
        if test "$variant" = ui-preflight-tree-mouse || test "$variant" = ui-preflight-tree-alias; then
            printf 'native archive tree\n' > "$run_dir/NOTE.TXT"
            mmd -i "$run_dir/boot.img" ::/SRC ::/SRC/EMPTY
            mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$run_dir/NOTE.TXT" ::/SRC/
            if test "$variant" = ui-preflight-tree-alias; then mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" ::/SRC/RMT0.CFG; fi
        fi
        if test "$variant" = ui-preflight-depth || test "$variant" = ui-preflight-limit; then
            mmd -i "$run_dir/boot.img" ::/SRC
            source_path=::/SRC; levels=16
            if test "$variant" = ui-preflight-limit; then levels=17; fi
            for ((level=0; level<levels; level++)); do source_path+=/N; mmd -i "$run_dir/boot.img" "$source_path"; done
            mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$source_path/INPUT.BIN"
        fi
    fi
    if [[ "$variant" = ui-backup-progress* ]]; then mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/PRGTEST.EXE" ::/; fi
    if [[ "$variant" = ui-restore-progress* ]] || test "$variant" = ui-preview-progress || test "$variant" = ui-compare-progress; then
        mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/RESTEST.EXE" ::/
    fi
    mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/NRMT3
    if test "$variant" = tree || test "$variant" = ui-verify-progress-tree || test "$variant" = ui-restore-tree || test "$variant" = ui-restore-progress-tree || test "$variant" = ui-replace-tree || test "$variant" = ui-compare-tree || [[ "$variant" = ui-search* ]] || [[ "$variant" = ui-directory* ]] || [[ "$variant" = ui-restore-directory* ]] || [[ "$variant" = ui-import* ]] || { test "$variant" = ui-backup-tree || test "$variant" = ui-backup-form-tree-mouse || test "$variant" = ui-backup-progress-tree-mouse; }; then
        printf 'native archive tree\n' > "$run_dir/NOTE.TXT"
        mmd -i "$run_dir/boot.img" ::/SRC
        mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$run_dir/NOTE.TXT" ::/SRC/
    fi
    if [[ "$variant" = ui-directory* ]]; then mmd -i "$run_dir/boot.img" ::/SRC/EMPTY; fi
    if test "$variant" = ui-preview-keep; then
        mmd -i "$run_dir/boot.img" ::/PREVIEW ::/PREVIEW/DATA
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/PREVIEW/DATA/INPUT.BIN
    fi
    if [[ "$variant" = ui-replace* ]] || test "$variant" = replace-service; then
        mmd -i "$run_dir/boot.img" ::/RECOVER ::/RECOVER/DATA
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/DATA/INPUT.BIN
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/KEEP.TXT
        mattrib -i "$run_dir/boot.img" +r ::/RECOVER/DATA/INPUT.BIN ::/RECOVER/KEEP.TXT
        if test "$variant" = ui-replace-tree; then
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/DATA/NOTE.TXT
            mattrib -i "$run_dir/boot.img" +r ::/RECOVER/DATA/NOTE.TXT
        fi
        if test "$variant" = ui-replace-staging; then
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" '::/RECOVER/OT$OLD.TMP'
        fi
    fi
    if [[ "$variant" = ui-compare* ]] || test "$variant" = compare-service; then
        mmd -i "$run_dir/boot.img" ::/RECOVER ::/RECOVER/DATA
        if test "$variant" != ui-compare-missing; then
            # Attribute differences intentionally do not affect content equality.
            mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" ::/RECOVER/DATA/INPUT.BIN
            if test "$variant" = ui-compare-mismatch || test "$variant" = ui-compare-short; then
                python3 - "$run_dir/INPUT.BIN" "$run_dir/CHANGED.BIN" "$variant" <<'PY_COMPARE'
import pathlib, sys
original = pathlib.Path(sys.argv[1]).read_bytes()
changed = bytearray(original)
if sys.argv[3] == 'ui-compare-short': del changed[-1]
else: changed[123] ^= 1
pathlib.Path(sys.argv[2]).write_bytes(changed)
PY_COMPARE
                mcopy -o -i "$run_dir/boot.img" "$run_dir/CHANGED.BIN" ::/RECOVER/DATA/INPUT.BIN
            fi
            mattrib -i "$run_dir/boot.img" +r ::/RECOVER/DATA/INPUT.BIN
        fi
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/KEEP.TXT
        if test "$variant" = ui-compare-tree; then
            # Unselected destination data may differ; only the archive is verified.
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/DATA/NOTE.TXT
        fi
        if test "$variant" = ui-compare-staging; then
            mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" '::/RECOVER/OT$OLD.TMP'
        fi
        cp "$run_dir/boot.img" "$run_dir/before.img"
    fi
    if [[ "$variant" = ui-import* ]] || test "$variant" = import-service; then
        python3 - "$run_dir" "$variant" <<'PY_IMPORT'
import pathlib, sys
root, variant = pathlib.Path(sys.argv[1]), sys.argv[2]
lists = {'VALID.LST': b'# selection only\r\ndata/note.txt\r\n',
         'UNKNOWN.LST': b'DATA/NOTE.TXT\nDATA/ABSENT.BIN\n',
         'BAD.LST': b'DATA/NOTE.TXT\nBAD\x00\n', 'DUP.LST': b'DATA/NOTE.TXT\ndata/note.txt\n',
         'EMPTY.LST': b'# no selected files\n', 'DIR.LST': b'DATA\n',
         'LIMIT.LST': (b'#' + b'x' * 254 + b'\n') * 513}
for name, contents in lists.items(): (root / name).write_bytes(contents)
source = {'ui-import-unknown': 'UNKNOWN.LST', 'ui-import-bad': 'BAD.LST',
          'ui-import-duplicate': 'DUP.LST', 'ui-import-empty': 'EMPTY.LST',
          'ui-import-directory': 'DIR.LST', 'ui-import-limit': 'LIMIT.LST'}.get(variant, 'VALID.LST')
(root / 'INPUT.LST').write_bytes(lists[source])
PY_IMPORT
        for input_list in VALID UNKNOWN BAD DUP EMPTY DIR LIMIT INPUT; do
            mcopy -i "$run_dir/boot.img" "$run_dir/$input_list.LST" ::/
        done
        if test "$variant" != ui-import-restore && test "$variant" != ui-import-mouse && test "$variant" != ui-import-paths-mouse; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    fi
    if [[ "$variant" = ui-backup* ]] || test "$variant" = backup-service; then
        mmd -i "$run_dir/boot.img" ::/CHECK ::/CHECK/DATA
        mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" ::/CHECK/DATA/INPUT.BIN
        if { test "$variant" = ui-backup-tree || test "$variant" = ui-backup-form-tree-mouse || test "$variant" = ui-backup-progress-tree-mouse; }; then
            mcopy -i "$run_dir/boot.img" "$run_dir/NOTE.TXT" ::/CHECK/DATA/NOTE.TXT
        fi
        if test "$variant" = ui-backup-depth || test "$variant" = ui-backup-depth-limit; then
            mmd -i "$run_dir/boot.img" ::/SRC
            source_path=::/SRC
            mirror_path=::/CHECK/DATA
            levels=16
            if test "$variant" = ui-backup-depth-limit; then levels=17; fi
            for ((level=0; level<levels; level++)); do
                source_path+=/N; mirror_path+=/N
                mmd -i "$run_dir/boot.img" "$source_path" "$mirror_path"
            done
            mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$source_path/INPUT.BIN"
            mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$mirror_path/INPUT.BIN"
        fi
        cp "$run_dir/boot.img" "$run_dir/before.img"
    fi
    if test "$variant" = ui-restore-existing; then
        mmd -i "$run_dir/boot.img" ::/RECOVER ::/RECOVER/DATA
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RECOVER/DATA/INPUT.BIN
    fi
    case "$variant" in
        ui-replace-cancel|ui-replace-disabled|ui-replace-rewind-fault|ui-replace-read-fault|ui-replace-staging|ui-restore-cancel|ui-restore-disabled|ui-restore-existing|ui-restore-invalid|ui-restore-rewind-fault)
            cp "$run_dir/boot.img" "$run_dir/before.img" ;;
    esac
    if test "$variant" = ui-preview-progress || [[ "$variant" = ui-preflight* ]] || [[ "$variant" = ui-profile* ]] || [[ "$variant" = ui-search* ]] || [[ "$variant" = ui-directory* ]] || test "$variant" = ui-paths-cancel || test "$variant" = ui-paths-disable; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    if test "$ui_mode" = 2; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    if test "$variant" = ui-mouse; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    if [[ "$variant" = ui-catfile* ]]; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    source "$ROOT/tests/catalog_cli_files.sh"
    source "$ROOT/tests/backup_set_files.sh"
    if [[ "$variant" = ui-verify-progress* ]]; then cp "$run_dir/boot.img" "$run_dir/before.img"; fi
    qemu_timeout=90
    if [[ "$variant" = *mouse* ]]; then qemu_timeout=180; fi
    set +e
    if test "$ui_mode" = 1; then
        (OUT="$run_dir"; if test "$variant" = ui-catfile-offline || test "$variant" = ui-catfile-large || test "$variant" = ui-catalog-start; then source "$ROOT/tests/catalog_offline_monitor.sh";
         elif [[ "$variant" = ui-verify-progress* ]]; then source "$ROOT/tests/verify_progress_monitor.sh";
         elif [[ "$variant" = ui-set* ]]; then source "$ROOT/tests/backup_set_monitor.sh";
         elif [[ "$variant" = ui-preflight* ]]; then source "$ROOT/tests/backup_preview_monitor.sh";
         elif [[ "$variant" = ui-profile* ]]; then source "$ROOT/tests/settings_ui_monitor.sh";
         elif test "$variant" = ui-mouse; then source "$ROOT/tests/mouse_catalog_monitor.sh";
         elif [[ "$variant" = ui-backup-progress* ]]; then source "$ROOT/tests/backup_progress_monitor.sh";
         elif test "$variant" = ui-backup-mouse; then source "$ROOT/tests/mouse_backup_monitor.sh";
         elif [[ "$variant" = ui-backup-form* ]]; then source "$ROOT/tests/backup_form_monitor.sh";
         elif [[ "$variant" = ui-backup* ]]; then source "$ROOT/tests/backup_ui_monitor.sh";
         else source "$ROOT/tests/native_ui_monitor.sh"; fi) | \
            timeout "$qemu_timeout" qemu-system-i386 -machine pc -m 16 -display none -monitor stdio \
            -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a \
            -serial file:"$run_dir/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
        monitor_status=("${PIPESTATUS[@]}")
        rc=${monitor_status[1]}
        test "${monitor_status[0]}" = 0 || { echo 'Native UI monitor failed' >&2; exit 1; }
    else
        timeout 60 qemu-system-i386 -machine pc -m 16 -display none \
            -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a \
            -serial file:"$run_dir/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
        rc=$?
    fi
    set -e
    test "$rc" = 1 || { echo "QEMU failed in $variant: $rc" >&2; exit 1; }
    source "$ROOT/tests/catalog_cli_check.sh"
    source "$ROOT/tests/backup_set_check.sh"
    if [[ "$variant" = ui-verify-progress* ]]; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if test "$variant" = ui-catfile || test "$variant" = ui-catfile-mouse; then
        mcopy -i "$run_dir/boot.img" ::/OPENTAPE.CAT "$run_dir/saved.cat"
        python3 "$ROOT/tests/check_catalog_file.py" "$run_dir/saved.cat"
        if mdir -i "$run_dir/boot.img" '::/OT$LIST.TMP' >/dev/null 2>&1; then echo 'Unexpected catalogue staging' >&2; exit 1; fi
    elif test "$variant" = ui-catfile-large; then
        mcopy -i "$run_dir/boot.img" ::/OPENTAPE.CAT "$run_dir/large.cat"
        python3 "$ROOT/tests/check_catalog_file.py" "$run_dir/large.cat" large
        if mdir -i "$run_dir/boot.img" '::/OT$LIST.TMP' >/dev/null 2>&1; then exit 1; fi
    elif [[ "$variant" = ui-catfile* ]]; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if test "$variant" = ui-preview-progress; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if [[ "$variant" = ui-preflight* ]]; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if [[ "$variant" = ui-profile* ]]; then
        if test "$variant" = ui-profile || test "$variant" = ui-profile-mouse; then
            mtype -i "$run_dir/boot.img" ::/OPENTAPE.CFG | cmp - "$run_dir/EXPECTED.CFG"
            if mdir -i "$run_dir/boot.img" '::/OT$LIST.TMP' >/dev/null 2>&1 || mdir -i "$run_dir/boot.img" ::/CANCEL.CFG >/dev/null 2>&1; then
                echo 'Profile export left staging or created cancelled output' >&2; exit 1
            fi
        else cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    fi
    if [[ "$variant" = ui-search* ]] || [[ "$variant" = ui-directory* ]] || test "$variant" = ui-paths-cancel || test "$variant" = ui-paths-disable; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if test "$variant" = ui-preview-paths; then
        printf 'DATA/INPUT.BIN\r\n' > "$run_dir/expected.lst"
        mtype -i "$run_dir/boot.img" ::/UI.LST | cmp - "$run_dir/expected.lst"
    fi
    if test "$ui_mode" = 2; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if test "$variant" = ui-mouse; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    case "$variant" in
        ui-replace-cancel|ui-replace-disabled|ui-replace-rewind-fault|ui-replace-read-fault|ui-replace-staging|ui-restore-cancel|ui-restore-disabled|ui-restore-existing|ui-restore-invalid|ui-restore-rewind-fault)
            cmp "$run_dir/before.img" "$run_dir/boot.img" ;;
    esac
    if [[ "$variant" = ui-compare* ]] || test "$variant" = compare-service; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi
    if { [[ "$variant" = ui-import* ]] && test "$variant" != ui-import-restore && test "$variant" != ui-import-mouse && test "$variant" != ui-import-paths-mouse; } || test "$variant" = import-service; then
        cmp "$run_dir/before.img" "$run_dir/boot.img"
    fi
    if [[ "$variant" = ui-backup* ]] || test "$variant" = backup-service; then
        cmp "$run_dir/before.img" "$run_dir/boot.img"
    fi
    ! grep -Fq 'NATIVE ARCHIVE CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'NATIVE ARCHIVE CLI PASSED' "$run_dir/serial.log"
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    if test "$variant" = backup-service; then
        python3 - "$run_dir/serial.log" <<'PY_QUIET'
import pathlib, sys
lines = pathlib.Path(sys.argv[1]).read_text().splitlines()
start, end = lines.index('Begin quiet backup service'), lines.index('Quiet backup service passed')
if end != start + 1: raise SystemExit('Quiet backup service wrote console output')
PY_QUIET
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    mtype -i "$run_dir/boot.img" ::/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
    if [[ "$variant" = ui-replace* ]] || test "$variant" = replace-service; then
        mtype -i "$run_dir/boot.img" ::/RECOVER/KEEP.TXT | cmp - "$ROOT/LICENSE"
        if test "$variant" = ui-replace-tree; then
            mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/NOTE.TXT | cmp - "$ROOT/LICENSE"
        fi
        case "$variant" in
            replace-service|ui-replace|ui-replace-legacy|ui-replace-fixed|ui-replace-tree|ui-replace-late-fault)
                mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
                if mdir -i "$run_dir/boot.img" '::/RECOVER/OT$*.TMP' >/dev/null 2>&1; then
                    echo 'Successful replacement left a staging/recovery file' >&2; exit 1
                fi ;;
        esac
    fi
    if test "$variant" = replace-service; then
        mtype -i "$run_dir/boot.img" ::/FRESH/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
    fi
    if [[ "$variant" = ui-restore-directory* ]]; then
        mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
        mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/NOTE.TXT | cmp - "$run_dir/NOTE.TXT"
        if mdir -i "$run_dir/boot.img" '::/RECOVER/OT$*.TMP' >/dev/null 2>&1; then
            echo 'Directory restore left staging/recovery file' >&2; exit 1
        fi
    fi
    case "$variant" in
        restore-service|ui-restore-progress|ui-restore-progress-mouse|ui-restore-progress-late|ui-restore-progress-tree|ui-restore|ui-restore-paths|ui-restore-legacy|ui-restore-fixed|ui-restore-late-fault)
            mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN" ;;
        ui-import-mouse|ui-import-paths-mouse|ui-import-restore|ui-restore-tree)
            mtype -i "$run_dir/boot.img" ::/RECOVER/DATA/NOTE.TXT | cmp - "$run_dir/NOTE.TXT"
            if mdir -i "$run_dir/boot.img" ::/RECOVER/DATA/INPUT.BIN >/dev/null 2>&1; then
                echo 'Filtered GUI restore extracted unmarked file' >&2; exit 1
            fi ;;
        ui-restore-read-fault)
            mdir -i "$run_dir/boot.img" ::/RECOVER >/dev/null
            if mdir -i "$run_dir/boot.img" ::/RECOVER/DATA/INPUT.BIN >/dev/null 2>&1; then
                echo 'Failed read published a file' >&2; exit 1
            fi ;;
    esac
    if test "$variant" = ui-restore-progress-tree; then
        if mdir -i "$run_dir/boot.img" ::/RECOVER/DATA/NOTE.TXT >/dev/null 2>&1; then
            echo 'Progress restore wrote an excluded file' >&2; exit 1
        fi
    fi
    if test "$ui_mode" != 0; then
        if test "$variant" = ui-preview-keep; then
            mtype -i "$run_dir/boot.img" ::/PREVIEW/DATA/INPUT.BIN | cmp - "$ROOT/LICENSE"
        else
            if mdir -i "$run_dir/boot.img" ::/PREVIEW > /dev/null 2>&1; then
                echo 'Read-only UI preview created destination' >&2; exit 1
            fi
        fi
        if test "$variant" = ui || test "$variant" = ui-legacy || test "$variant" = fixed-ui; then
            mtype -i "$run_dir/boot.img" ::/UI.LST | grep -Fq 'DATA/INPUT.BIN'
        fi
    elif test "$backup_exit:$read_exit" = 0:0; then
        mtype -i "$run_dir/boot.img" ::/OUT/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
        if test "$variant" = tree || test "$variant" = ui-verify-progress-tree || test "$variant" = ui-restore-tree || test "$variant" = ui-restore-progress-tree || test "$variant" = ui-replace-tree; then
            mtype -i "$run_dir/boot.img" ::/OUT/DATA/NOTE.TXT | cmp - "$run_dir/NOTE.TXT"
        fi
        mtype -i "$run_dir/boot.img" ::/NATIVE.LST | grep -Fq 'DATA/INPUT.BIN'
    fi
    printf 'Native archive CLI: %s %s passed\n' "$MAXLANG" "$variant"
done

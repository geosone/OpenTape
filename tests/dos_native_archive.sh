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
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal tree replace legacy combined eom eod corrupt read-hard write-short flush mark end ui ui-failed ui-legacy ui-unfinished)
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
        fixed) fixed_unit=512; calls=125 ;;
        fixed-ui) fixed_unit=512; ui_mode=1; calls=53 ;;
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
        ui) ui_mode=1; calls=28 ;;
        ui-legacy) ui_mode=1; calls=28 ;;
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
    if test "$variant" = tree; then archive_path=DATA; fi
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'OPENTAPE --native --status > AUX' 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED'
        "OPENTAPE --native $backup_kind $backup_source --as $archive_path --device NRMT3 --overwrite --volume TEST > AUX"
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
        "OPENTAPE --native $backup_kind $backup_source --as $archive_path --device NRMT0 --overwrite --volume TEST > AUX"
        "IF ERRORLEVEL $((backup_exit+1)) GOTO FAILED")
    if test "$ui_mode" = 1; then
        if test "$backup_exit" = 2; then lines+=('IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
        ui_command='OPENTAPE --native --ui --selection-out UI.LST > AUX'
        if test "$variant" = ui-legacy; then ui_command='OPENTAPE --ui --selection-out UI.LST > AUX'; fi
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
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" "$run_dir/MOCKASPI.SYS" \
        "$OPENASPI/build/dos/ASPITAP.SYS" "$OUT/bin-$MAXLANG/OPENTAPE.EXE" \
        "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" "$run_dir/INPUT.BIN" ::/
    mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/NRMT3
    if test "$variant" = tree; then
        printf 'native archive tree\n' > "$run_dir/NOTE.TXT"
        mmd -i "$run_dir/boot.img" ::/SRC
        mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$run_dir/NOTE.TXT" ::/SRC/
    fi
    set +e
    if test "$ui_mode" = 1; then
        (OUT="$run_dir"; source "$ROOT/tests/native_ui_monitor.sh") | \
            timeout 90 qemu-system-i386 -machine pc -m 16 -display none -monitor stdio \
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
    ! grep -Fq 'NATIVE ARCHIVE CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'NATIVE ARCHIVE CLI PASSED' "$run_dir/serial.log"
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    mtype -i "$run_dir/boot.img" ::/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
    if test "$ui_mode" = 1; then
        if test "$variant" = ui || test "$variant" = ui-legacy || test "$variant" = fixed-ui; then
            mtype -i "$run_dir/boot.img" ::/UI.LST | grep -Fq 'DATA/INPUT.BIN'
        fi
    elif test "$backup_exit:$read_exit" = 0:0; then
        mtype -i "$run_dir/boot.img" ::/OUT/DATA/INPUT.BIN | cmp - "$run_dir/INPUT.BIN"
        if test "$variant" = tree; then
            mtype -i "$run_dir/boot.img" ::/OUT/DATA/NOTE.TXT | cmp - "$run_dir/NOTE.TXT"
        fi
        mtype -i "$run_dir/boot.img" ::/NATIVE.LST | grep -Fq 'DATA/INPUT.BIN'
    fi
    printf 'Native archive CLI: %s %s passed\n' "$MAXLANG" "$variant"
done

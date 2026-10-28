#!/bin/bash
# Synchronous logical LOCATE through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-wide-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTWIDE.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy eof lonu mpu both reserved short lost reserved-byte residual-high largepart pos-host host unsupported deferred contradict descriptor busy)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_POSITION_TEST -DNATIVE_LOCATE_TEST -DNATIVE_WIDE_TEST)
    argument=normal calls=24 cli_calls=10 writes=1 position_exit=0
    case "$variant" in
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL); argument=legacy; position_exit=2 ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof; calls=3; writes=0 ;;
        lonu) defines+=(-DNATIVE_WIDE_CASE=1); argument='normal 1'; position_exit=2 ;;
        mpu) defines+=(-DNATIVE_WIDE_CASE=2); argument='normal 2' ;;
        both) defines+=(-DNATIVE_WIDE_CASE=3); argument='normal 3'; position_exit=2 ;;
        reserved) defines+=(-DNATIVE_WIDE_CASE=4); argument=malformed; position_exit=2 ;;
        short) defines+=(-DNATIVE_WIDE_CASE=5); argument=malformed; position_exit=2 ;;
        lost) defines+=(-DNATIVE_WIDE_CASE=6); argument=malformed; position_exit=2 ;;
        reserved-byte) defines+=(-DNATIVE_WIDE_CASE=7); argument=malformed; position_exit=2 ;;
        residual-high) defines+=(-DNATIVE_WIDE_CASE=8); argument=malformed; position_exit=2 ;;
        largepart) defines+=(-DNATIVE_WIDE_CASE=9); argument='normal 9' ;;
        pos-host) defines+=(-DNATIVE_WIDE_POS_FAULT=1); argument=posfault; position_exit=2 ;;
        host) defines+=(-DNATIVE_WIDE_FAULT=1); argument='fault 1' ;;
        unsupported) defines+=(-DNATIVE_WIDE_FAULT=2); argument='fault 4' ;;
        deferred) defines+=(-DNATIVE_WIDE_FAULT=3); argument='fault 2' ;;
        contradict) defines+=(-DNATIVE_WIDE_FAULT=4); argument='fault 1' ;;
        descriptor) defines+=(-DNATIVE_WIDE_FAULT=5); argument='fault 4' ;;
        busy) defines+=(-DNATIVE_WIDE_FAULT=6); argument='fault 2' ;;
        *) echo "Unknown wide fixture: $variant" >&2; exit 1 ;;
    esac
    if [[ "$argument" = fault* ]]; then calls=6; cli_calls=6; writes=0; fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -DEXPECTED_WRITES="$writes" -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$cli_calls" -o "$run_dir/CLI.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' 'DEVICE=ASPITAP.SYS -w' 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTWIDE $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-locate64' '--native-locate64 -1' '--native-locate64 +1' \
        '--native-locate64 18446744073709551616' '--native-locate 4294967296' '--native-locate64 99999999999999999999' '--native-locate64 x' \
        '--native-locate64 1 --partition 256' '--native-locate64 1 --partition -1' \
        '--native-locate64 1 --partition 0 --partition 0' '--native-locate64 1 --partition' \
        '--native-locate64 1 --refresh' '--native-locate64 1 --device RMT0' \
        '--native-position64 --partition 0' '--native-ready --partition 0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-locate64 0 --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    if [[ "$argument" = fault* ]]; then
        for command in '--native-locate64 18446744073709551615 --partition 255' '--native-locate64 0'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        done
        lines+=('OPENTAPE --native-position64 > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        for command in '--native-ready' '--native-rewind' '--native-locate64 18446744073709551615 --partition 255' '--native-position64'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
    else
        for command in '--native-locate64 0 --partition 2' '--native-locate64 4294967296' \
            '--native-locate64 18446744073709551615 --partition 255' '--native-locate64 9223372036854775808' \
            '--native-locate64 0 --partition 0'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED'
                'OPENTAPE --native-position64 > AUX')
            if test "$position_exit" = 0; then lines+=('IF ERRORLEVEL 1 GOTO FAILED');
            else lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
        done
        lines+=('OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE WIDE CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE WIDE CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTWIDE.EXE" \
        "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/CLI.COM" "$run_dir/QEXIT.COM" ::/
    mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/NRMT3
    set +e
    timeout 60 qemu-system-i386 -machine pc -m 16 -display none \
        -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a \
        -serial file:"$run_dir/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
        > "$run_dir/process.log" 2>&1
    result=$?
    set -e
    test "$result" = 1
    ! grep -Fq 'NATIVE WIDE CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native wide tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE WIDE CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    if [[ "$argument" = fault* ]]; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Partition=255 Block=16045690985374415566 Flags=00' "$run_dir/serial.log"
        else
            grep -Fq 'Partition=255 block=16045690985374415566 flags=00' "$run_dir/serial.log"
        fi
    elif test "$position_exit" = 0 && test "$variant" != largepart; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Partition=255 Block=9223372036854775808 Flags=' "$run_dir/serial.log"
        else
            grep -Fq 'Partition=255 block=9223372036854775808 flags=' "$run_dir/serial.log"
        fi
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native wide DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

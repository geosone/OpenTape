#!/bin/bash
# Synchronous logical LOCATE through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-locate-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTLOC.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy eof host unsupported deferred contradict target busy mark descriptor)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_POSITION_TEST -DNATIVE_LOCATE_TEST)
    argument=normal calls=23 cli_calls=10 writes=1 position_exit=0
    case "$variant" in
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL); argument=legacy; position_exit=2 ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof; calls=3; writes=0 ;;
        host) defines+=(-DNATIVE_LOCATE_FAULT=1); argument='fault 1' ;;
        unsupported) defines+=(-DNATIVE_LOCATE_FAULT=2); argument='fault 4' ;;
        deferred) defines+=(-DNATIVE_LOCATE_FAULT=3); argument='fault 2' ;;
        contradict) defines+=(-DNATIVE_LOCATE_FAULT=4); argument='fault 1' ;;
        target) defines+=(-DNATIVE_LOCATE_FAULT=5); argument='fault 2' ;;
        busy) defines+=(-DNATIVE_LOCATE_FAULT=6); argument='fault 2' ;;
        mark) defines+=(-DNATIVE_LOCATE_FAULT=7); argument='fault 2' ;;
        descriptor) defines+=(-DNATIVE_LOCATE_FAULT=8); argument='fault 4' ;;
        *) echo "Unknown LOCATE fixture: $variant" >&2; exit 1 ;;
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTLOC $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-locate' '--native-locate -1' '--native-locate +1' \
        '--native-locate 4294967296' '--native-locate 99999999999999999999' '--native-locate x' \
        '--native-locate 1 --partition 256' '--native-locate 1 --partition -1' \
        '--native-locate 1 --partition 0 --partition 0' '--native-locate 1 --partition' \
        '--native-locate 1 --refresh' '--native-locate 1 --device RMT0' \
        '--native-position --partition 0' '--native-ready --partition 0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-locate 0 --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    if [[ "$argument" = fault* ]]; then
        for command in '--native-locate 4294967295 --partition 255' '--native-locate 0'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        done
        lines+=('OPENTAPE --native-position > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        for command in '--native-ready' '--native-rewind' '--native-locate 4294967295 --partition 255' '--native-position'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
    else
        for command in '--native-locate 0 --partition 2' '--native-locate 0' \
            '--native-locate 4294967295 --partition 255' '--native-locate 2147483648' \
            '--native-locate 0 --partition 0'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED'
                'OPENTAPE --native-position > AUX')
            if test "$position_exit" = 0; then lines+=('IF ERRORLEVEL 1 GOTO FAILED');
            else lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
        done
        lines+=('OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE LOCATE CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE LOCATE CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTLOC.EXE" \
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
    ! grep -Fq 'NATIVE LOCATE CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native LOCATE tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE LOCATE CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    if [[ "$argument" = fault* ]]; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Partition=255 erster Block=3735928559 letzter Block=3735928559 Flags=00' "$run_dir/serial.log"
        else
            grep -Fq 'Partition=255 first block=3735928559 last block=3735928559 flags=00' "$run_dir/serial.log"
        fi
    elif test "$position_exit" = 0; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Partition=255 erster Block=2147483648 letzter Block=2147483648 Flags=00' "$run_dir/serial.log"
        else
            grep -Fq 'Partition=255 first block=2147483648 last block=2147483648 flags=00' "$run_dir/serial.log"
        fi
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native LOCATE DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

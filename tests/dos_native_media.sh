#!/bin/bash
# Synchronous LOAD/UNLOAD/RETENSION through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-media-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTMEDIA.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy load-host load-unsupported load-deferred load-contradict unload-target unload-busy unload-mark)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST)
    argument='' calls=17 cli_calls=8 first='--native-load'
    case "$variant" in
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL) ;;
        load-host) defines+=(-DNATIVE_MEDIA_FAULT=1); argument='10 1' ;;
        load-unsupported) defines+=(-DNATIVE_MEDIA_FAULT=2); argument='10 4' ;;
        load-deferred) defines+=(-DNATIVE_MEDIA_FAULT=3); argument='10 2' ;;
        load-contradict) defines+=(-DNATIVE_MEDIA_FAULT=4); argument='10 1' ;;
        unload-target) defines+=(-DNATIVE_MEDIA_FAULT=5); argument='11 2'; first='--native-unload' ;;
        unload-busy) defines+=(-DNATIVE_MEDIA_FAULT=6); argument='11 2'; first='--native-unload' ;;
        unload-mark) defines+=(-DNATIVE_MEDIA_FAULT=7); argument='11 2'; first='--native-unload' ;;
        *) echo "Unknown media fixture: $variant" >&2; exit 1 ;;
    esac
    writes=1
    if test -n "$argument"; then calls=7; writes=0; fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -DEXPECTED_WRITES="$writes" -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$cli_calls" -DEXPECTED_RETENSIONS=1 -o "$run_dir/CLI.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' 'DEVICE=ASPITAP.SYS -w' 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTMEDIA $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-load 1' '--native-unload 1' '--native-retension 1' '--native-load --refresh' \
        '--native-unload --device NRMT0 --device NRMT0' '--native-load --device RMT0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-load --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    if test -n "$argument"; then
        for command in "$first" '--native-status' '--native-space-records 1'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        done
        lines+=('OPENTAPE --native-load > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native-unload > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    else
        lines+=('OPENTAPE --native-unload > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    for command in '--native-ready' '--native-rewind'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    done
    if test -z "$argument"; then
        lines+=('OPENTAPE --native --verify --all > AUX' 'IF ERRORLEVEL 1 GOTO READFAILED'
            'GOTO FAILED' ':READFAILED')
    fi
    lines+=('OPENTAPE --native-load > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'OPENTAPE --native-retension > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    if test -z "$argument"; then
        lines+=('OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    fi
    lines+=('OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE MEDIA CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE MEDIA CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTMEDIA.EXE" \
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
    ! grep -Fq 'NATIVE MEDIA CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native media tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE MEDIA CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native media DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

#!/bin/bash
# Non-moving PREVENT/ALLOW through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-lock-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTLOCK.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy eof lock-host lock-unsupported lock-deferred lock-contradict unlock-target unlock-busy unlock-mark lock-attention)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_LOCK_TEST)
    argument='' calls=20 cli_calls=9 first='--native-lock'
    case "$variant" in
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL) ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof ;;
        lock-host) defines+=(-DNATIVE_LOCK_FAULT=1); argument='12 1' ;;
        lock-unsupported) defines+=(-DNATIVE_LOCK_FAULT=2); argument='12 4' ;;
        lock-deferred) defines+=(-DNATIVE_LOCK_FAULT=3); argument='12 2' ;;
        lock-contradict) defines+=(-DNATIVE_LOCK_FAULT=4); argument='12 1' ;;
        unlock-target) defines+=(-DNATIVE_LOCK_FAULT=5); argument='13 2'; first='--native-unlock' ;;
        unlock-busy) defines+=(-DNATIVE_LOCK_FAULT=6); argument='13 2'; first='--native-unlock' ;;
        unlock-mark) defines+=(-DNATIVE_LOCK_FAULT=7); argument='13 2'; first='--native-unlock' ;;
        lock-attention) defines+=(-DNATIVE_LOCK_FAULT=8); argument=attention ;;
        *) echo "Unknown removal fixture: $variant" >&2; exit 1 ;;
    esac
    writes=1
    if test -n "$argument"; then calls=4; cli_calls=4; writes=0; fi
    if test "$variant" = eof; then calls=3; cli_calls=9; fi
    if test "$variant" = lock-attention; then calls=4; cli_calls=4; fi
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTLOCK $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-lock 1' '--native-unlock 1' '--native-lock --refresh' \
        '--native-unlock --device NRMT0 --device NRMT0' '--native-lock --device RMT0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-lock --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    if test "$variant" = lock-attention; then
        # UNIT ATTENTION latches FAILED: status reports it until REWIND.
        lines+=('OPENTAPE --native-lock > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
            'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        for command in '--native-ready' '--native-rewind' '--native-status' '--native-unlock'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
    elif test -n "$argument" && test "$variant" != eof; then
        lines+=("OPENTAPE $first > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
            'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
        for command in '--native-ready' '--native-lock' '--native-unlock'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
    else
        for command in '--native-lock' '--native-rewind'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
        lines+=('OPENTAPE --native-unload > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
            'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        for command in '--native-unlock' '--native-unload' '--native-load' '--native-lock' '--native-unlock' '--native-rewind'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
        done
    fi
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE LOCK CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE LOCK CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTLOCK.EXE" \
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
    ! grep -Fq 'NATIVE LOCK CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native lock tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE LOCK CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native lock DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

#!/bin/bash
# Read-only TapeAlert log page through installed provider, SDK and CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-alerts-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTALERT.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal all clear empty ds unknown legacy eof short subpage header duplicate unsorted size value control trailing residual-high residual-lost oversize zero-code host unsupported deferred contradict target busy descriptor)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_ALERTS_TEST)
    argument=known calls=6 cli_calls=2 writes=1 alerts_exit=0
    case "$variant" in
        normal) ;;
        all) defines+=(-DNATIVE_ALERTS_KIND=1); argument=all ;;
        clear) defines+=(-DNATIVE_ALERTS_KIND=2); argument=clear ;;
        empty) defines+=(-DNATIVE_ALERTS_KIND=3); argument=empty ;;
        unknown) defines+=(-DNATIVE_ALERTS_KIND=15) ;;
        ds) defines+=(-DNATIVE_ALERTS_KIND=16) ;;
        legacy) defines+=(-DNO_RESIDUAL); argument=legacy; alerts_exit=2 ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof; calls=2; writes=0 ;;
        short|subpage|header|duplicate|unsorted|size|value|control|trailing|residual-high|residual-lost|oversize|zero-code)
            case "$variant" in
                short) k=4;; subpage) k=5;; header) k=6;; duplicate) k=7;; unsorted) k=8;; size) k=9;;
                value) k=10;; control) k=11;; trailing) k=12;; residual-high) k=13;; residual-lost) k=14;;
                oversize) k=17;; zero-code) k=18;;
            esac
            defines+=(-DNATIVE_ALERTS_KIND="$k"); argument=malformed; alerts_exit=2 ;;
        host|unsupported|deferred|contradict|target|busy|descriptor)
            case "$variant" in
                host) k=1; d=1;; unsupported) k=2; d=4;; deferred) k=3; d=2;; contradict) k=4; d=1;;
                target) k=5; d=2;; busy) k=6; d=2;; descriptor) k=7; d=4;;
            esac
            defines+=(-DNATIVE_ALERTS_FAULT="$k"); argument="fault $d"; alerts_exit=2 ;;
        *) echo "Unknown partitions fixture: $variant" >&2; exit 1 ;;
    esac
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTALERT $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-alerts' '--native-alerts 1' '--native-alerts --refresh' \
        '--native-alerts --device NRMT0' '--native-alerts --allow-clear --allow-clear' \
        '--native-alerts --allow-clear --refresh' '--native-alerts --allow-clear --device NRMT0 --device NRMT0' \
        '--native-alerts --allow-clear --device RMT0' '--native-ready --allow-clear'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-alerts --allow-clear --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
        'OPENTAPE --native-alerts --allow-clear > AUX')
    if test "$alerts_exit" = 0; then lines+=('IF ERRORLEVEL 1 GOTO FAILED');
    else lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
    # An unsupported/malformed/unknown observation must not poison the session.
    lines+=('OPENTAPE --native-status > AUX')
    if test "$variant" = retained; then
        lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    else lines+=('IF ERRORLEVEL 1 GOTO FAILED'); fi
    lines+=('OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE ALERTS CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE ALERTS CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTALERT.EXE" \
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
    ! grep -Fq 'NATIVE ALERTS CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native alerts tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE ALERTS CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    if test "$alerts_exit" = 0; then
        if test "$MAXLANG" = GR; then grep -Fq 'Lesen kann Warnflags im Laufwerk quittieren.' "$run_dir/serial.log";
        else grep -Fq 'Reading may acknowledge drive warning flags.' "$run_dir/serial.log"; fi
        if test "$variant" = normal || test "$variant" = unknown || test "$variant" = ds; then
            if test "$MAXLANG" = GR; then grep -Fq 'Gemeldete Flags: 5, aktiv: 3' "$run_dir/serial.log";
            else grep -Fq 'Reported flags: 5, active: 3' "$run_dir/serial.log"; fi
        fi
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native alerts DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

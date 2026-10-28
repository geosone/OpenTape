#!/bin/bash
# Read-only MODE SENSE partition layout through installed provider, SDK and CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-partitions-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTPART.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal saved pending empty legacy eof reserved odd descriptor short subpage missing duplicate trailing residual-high residual-lost host unsupported deferred contradict target busy mark)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_PARTITIONS_TEST)
    argument=known calls=6 cli_calls=2 writes=1 partitions_exit=0
    case "$variant" in
        normal) ;;
        saved) defines+=(-DNATIVE_PARTITIONS_KIND=1) ;;
        pending) defines+=(-DNATIVE_PARTITIONS_KIND=2); argument=pending ;;
        empty) defines+=(-DNATIVE_PARTITIONS_KIND=13); argument=empty ;;
        legacy) defines+=(-DNO_RESIDUAL); argument=legacy; partitions_exit=2 ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof; calls=2; writes=0 ;;
        reserved|odd|descriptor|short|subpage|missing|duplicate|trailing|residual-high|residual-lost)
            case "$variant" in
                reserved) k=3;; odd) k=4;; descriptor) k=5;; short) k=6;; subpage) k=7;;
                missing) k=8;; duplicate) k=9;; trailing) k=10;; residual-high) k=11;; residual-lost) k=12;;
            esac
            defines+=(-DNATIVE_PARTITIONS_KIND="$k"); argument=malformed; partitions_exit=2 ;;
        host|unsupported|deferred|contradict|target|busy|mark)
            case "$variant" in
                host) k=1; d=1;; unsupported) k=2; d=4;; deferred) k=3; d=2;; contradict) k=4; d=1;;
                target) k=5; d=2;; busy) k=6; d=2;; mark) k=7; d=2;;
            esac
            defines+=(-DNATIVE_PARTITIONS_FAULT="$k"); argument="fault $d"; partitions_exit=2 ;;
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTPART $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-partitions 1' '--native-partitions --refresh' \
        '--native-partitions --device NRMT0 --device NRMT0' '--native-partitions --device RMT0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-partitions --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
        'OPENTAPE --native-partitions > AUX')
    if test "$partitions_exit" = 0; then lines+=('IF ERRORLEVEL 1 GOTO FAILED');
    else lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
    # An unsupported/malformed/unknown observation must not poison the session.
    lines+=('OPENTAPE --native-status > AUX')
    if test "$variant" = retained; then
        lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    else lines+=('IF ERRORLEVEL 1 GOTO FAILED'); fi
    lines+=('OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE PARTITIONS CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE PARTITIONS CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTPART.EXE" \
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
    ! grep -Fq 'NATIVE PARTITIONS CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native partitions tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE PARTITIONS CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    if test "$partitions_exit" = 0; then
        if test "$MAXLANG" = GR; then grep -Fq 'Gemeldetes Layout; kann noch ausstehend sein.' "$run_dir/serial.log";
        else grep -Fq 'Reported layout; may be pending.' "$run_dir/serial.log"; fi
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native partitions DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

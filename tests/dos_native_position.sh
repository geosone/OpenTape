#!/bin/bash
# Non-moving logical READ POSITION through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-position-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTPOS.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal retained unknown perr bop edge legacy eof reserved reserved-byte short residual-high residual-lost host unsupported deferred contradict target busy mark)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_POSITION_TEST)
    argument=known calls=6 cli_calls=2 writes=1 position_exit=0
    case "$variant" in
        normal) ;;
        retained) argument=retained; calls=0; writes=0 ;;
        unknown) defines+=(-DNATIVE_POSITION_KIND=1); argument=unknown; position_exit=2 ;;
        perr) defines+=(-DNATIVE_POSITION_KIND=2); argument=perr; position_exit=2 ;;
        bop) defines+=(-DNATIVE_POSITION_KIND=3); argument=bop ;;
        edge) defines+=(-DNATIVE_POSITION_KIND=4); argument=edge ;;
        legacy) defines+=(-DNO_RESIDUAL); argument=legacy; position_exit=2 ;;
        eof) defines+=(-DNATIVE_LOCK_EOF -DNATIVE_READ_CASE=3); argument=eof; calls=2; writes=0 ;;
        reserved) defines+=(-DNATIVE_POSITION_KIND=5); argument=malformed; position_exit=2 ;;
        reserved-byte) defines+=(-DNATIVE_POSITION_KIND=6); argument=malformed; position_exit=2 ;;
        short) defines+=(-DNATIVE_POSITION_KIND=7); argument=malformed; position_exit=2 ;;
        residual-high) defines+=(-DNATIVE_POSITION_KIND=8); argument=malformed; position_exit=2 ;;
        residual-lost) defines+=(-DNATIVE_POSITION_KIND=9); argument=malformed; position_exit=2 ;;
        host) defines+=(-DNATIVE_POSITION_FAULT=1); argument='fault 1'; position_exit=2 ;;
        unsupported) defines+=(-DNATIVE_POSITION_FAULT=2); argument='fault 4'; position_exit=2 ;;
        deferred) defines+=(-DNATIVE_POSITION_FAULT=3); argument='fault 2'; position_exit=2 ;;
        contradict) defines+=(-DNATIVE_POSITION_FAULT=4); argument='fault 1'; position_exit=2 ;;
        target) defines+=(-DNATIVE_POSITION_FAULT=5); argument='fault 2'; position_exit=2 ;;
        busy) defines+=(-DNATIVE_POSITION_FAULT=6); argument='fault 2'; position_exit=2 ;;
        mark) defines+=(-DNATIVE_POSITION_FAULT=7); argument='fault 2'; position_exit=2 ;;
        *) echo "Unknown position fixture: $variant" >&2; exit 1 ;;
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTPOS $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in '--native-position 1' '--native-position --refresh' \
        '--native-position --device NRMT0 --device NRMT0' '--native-position --device RMT0'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    lines+=('OPENTAPE --native-position --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'
        'OPENTAPE --native-position > AUX')
    if test "$position_exit" = 0; then lines+=('IF ERRORLEVEL 1 GOTO FAILED');
    else lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED'); fi
    # An unsupported/malformed/unknown observation must not poison the session.
    lines+=('OPENTAPE --native-status > AUX')
    if test "$variant" = retained; then
        lines+=('IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    else lines+=('IF ERRORLEVEL 1 GOTO FAILED'); fi
    lines+=('OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE POSITION CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE POSITION CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTPOS.EXE" \
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
    ! grep -Fq 'NATIVE POSITION CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native position tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE POSITION CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    if test "$position_exit" = 2; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Keine gesicherte logische Bandposition verfuegbar.' "$run_dir/serial.log"
        else
            grep -Fq 'No verified logical tape position available.' "$run_dir/serial.log"
        fi
    elif test "$variant" = normal || test "$variant" = eof; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Partition=2 erster Block=2309737967 letzter Block=4275878552 Flags=00' "$run_dir/serial.log"
        else
            grep -Fq 'Partition=2 first block=2309737967 last block=4275878552 flags=00' "$run_dir/serial.log"
        fi
    fi
    if test "$variant" = retained; then
        if test "$MAXLANG" = GR; then
            grep -Fq 'Native Sitzung fehlgeschlagen oder unvollstaendig' "$run_dir/serial.log"
        else
            grep -Fq 'Native session failed or unfinished' "$run_dir/serial.log"
        fi
    fi
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Native position DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

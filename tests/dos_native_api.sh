#!/bin/bash
# Native OpenTape consumer/provider integration; memory-only tape, own images.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-api-dos}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
MAXLANG=${MAXLANG:-US}
test -f "$OPENASPI/include/openaspi/tape_api.h"
make -C "$OPENASPI" all >/dev/null
mkdir -p "$OUT"
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NAPIUT.EXE" "$OUT/bin-$MAXLANG/NTREAD.EXE" "$OUT/bin-$MAXLANG/NTWRITE.EXE" "$OUT/bin-$MAXLANG/NTFIX.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy partial short control-host control-target control-deferred control-unsupported control-contradict
    read-fixed read-short read-filemark read-data-filemark read-eom read-host read-residual
    read-deferred read-conflict read-truncated read-partial-error read-eod read-empty
    read-short-legacy read-filemark-legacy
    write-normal write-legacy write-fixed write-short write-host write-protect
    write-eom write-residual write-deferred write-contradict write-partial
    write-eom-full write-protect-legacy write-flush write-mark write-end
    fixed-normal fixed-legacy fixed-read-mark fixed-read-data-mark fixed-read-eom
    fixed-read-unaligned fixed-read-conflict fixed-read-overflow fixed-read-ili
    fixed-read-medium fixed-read-host fixed-read-deferred fixed-read-large
    fixed-mark-legacy fixed-noinfo-legacy fixed-overflow-legacy
    fixed-write-short fixed-write-protect fixed-write-unaligned fixed-write-host
    fixed-write-deferred fixed-write-eom-full fixed-write-eom-partial fixed-write-medium
    fixed-protect-legacy)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST)
    query_exit=0 control_exit=0 calls=8 read_case=0 write_case=-1 finish_fault=0 legacy_write=0 writer_calls=0
    fixed_mode=0 fixed_read=0 fixed_write=0 fixed_legacy=0
    case "$variant" in
        fixed-*)
            defines+=(-DNATIVE_FIXED_IO_TEST -DNATIVE_FIXED_LIMITS)
            calls=12; read_case=13; fixed_mode=1
            case "$variant" in
                fixed-normal) ;;
                fixed-legacy) defines+=(-DNO_RESIDUAL); fixed_legacy=1 ;;
                fixed-read-mark) fixed_read=1 ;;
                fixed-read-data-mark) fixed_read=2 ;;
                fixed-read-eom) fixed_read=3 ;;
                fixed-read-unaligned) fixed_read=4 ;;
                fixed-read-conflict) fixed_read=5 ;;
                fixed-read-overflow) fixed_read=6 ;;
                fixed-read-ili) fixed_read=7 ;;
                fixed-read-medium) fixed_read=8 ;;
                fixed-read-host) fixed_read=9 ;;
                fixed-read-deferred) fixed_read=10 ;;
                fixed-read-large) fixed_read=11 ;;
                fixed-mark-legacy) fixed_read=2; fixed_legacy=1; defines+=(-DNO_RESIDUAL) ;;
                fixed-noinfo-legacy) fixed_read=12; fixed_legacy=1; defines+=(-DNO_RESIDUAL) ;;
                fixed-overflow-legacy) fixed_read=6; fixed_legacy=1; defines+=(-DNO_RESIDUAL) ;;
                fixed-write-short) fixed_write=1; calls=10 ;;
                fixed-write-protect) fixed_write=2; calls=10 ;;
                fixed-write-unaligned) fixed_write=3; calls=10 ;;
                fixed-write-host) fixed_write=4; calls=10 ;;
                fixed-write-deferred) fixed_write=5; calls=10 ;;
                fixed-write-eom-full) fixed_write=6; calls=10 ;;
                fixed-write-eom-partial) fixed_write=7; calls=10 ;;
                fixed-write-medium) fixed_write=8; calls=10 ;;
                fixed-protect-legacy) fixed_write=2; calls=10; fixed_legacy=1; defines+=(-DNO_RESIDUAL) ;;
                *) echo "Unknown fixed fixture: $variant" >&2; exit 1 ;;
            esac
            if test "$fixed_read" -gt 0; then defines+=("-DNATIVE_FIXED_READ_CASE=$fixed_read"); fi
            if test "$fixed_write" -gt 0; then defines+=("-DNATIVE_FIXED_WRITE_CASE=$fixed_write"); fi
            ;;
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL) ;;
        partial) defines+=(-DNATIVE_NO_COMP); query_exit=2 ;;
        short) defines+=(-DNATIVE_SHORT); query_exit=2; calls=5 ;;
        write-normal) writer_calls=8; write_case=0 ;;
        write-legacy) defines+=(-DNO_RESIDUAL); writer_calls=8; write_case=0 ;;
        write-fixed) defines+=(-DNATIVE_FIXED_LIMITS); calls=6; read_case=13; write_case=10 ;;
        write-short) defines+=(-DNATIVE_WRITE_CASE=1); write_case=1; writer_calls=4 ;;
        write-host) defines+=(-DNATIVE_WRITE_CASE=2); write_case=2; writer_calls=4 ;;
        write-protect) defines+=(-DNATIVE_WRITE_CASE=3); write_case=3; writer_calls=4 ;;
        write-eom) defines+=(-DNATIVE_WRITE_CASE=4); write_case=4; writer_calls=4 ;;
        write-residual) defines+=(-DNATIVE_WRITE_CASE=5); write_case=5; writer_calls=4 ;;
        write-deferred) defines+=(-DNATIVE_WRITE_CASE=6); write_case=6; writer_calls=4 ;;
        write-contradict) defines+=(-DNATIVE_WRITE_CASE=7); write_case=7; writer_calls=4 ;;
        write-partial) defines+=(-DNATIVE_WRITE_CASE=8); write_case=8; writer_calls=4 ;;
        write-eom-full) defines+=(-DNATIVE_WRITE_CASE=9); write_case=9; writer_calls=4 ;;
        write-protect-legacy) defines+=(-DNO_RESIDUAL -DNATIVE_WRITE_CASE=3); write_case=3; writer_calls=4; legacy_write=1 ;;
        write-flush) defines+=(-DNATIVE_FINISH_FAULT=1); write_case=0; finish_fault=1; writer_calls=5 ;;
        write-mark) defines+=(-DNATIVE_FINISH_FAULT=2); write_case=0; finish_fault=2; writer_calls=6 ;;
        write-end) defines+=(-DNATIVE_FINISH_FAULT=3); write_case=0; finish_fault=3; writer_calls=7 ;;
        read-fixed) defines+=(-DNATIVE_FIXED_LIMITS); read_case=13; calls=6 ;;
        read-short) defines+=(-DNATIVE_READ_CASE=1); read_case=1 ;;
        read-filemark) defines+=(-DNATIVE_READ_CASE=2); read_case=2 ;;
        read-data-filemark) defines+=(-DNATIVE_READ_CASE=3); read_case=3 ;;
        read-eom) defines+=(-DNATIVE_READ_CASE=4); read_case=4 ;;
        read-host) defines+=(-DNATIVE_READ_CASE=5); read_case=5; calls=7 ;;
        read-residual) defines+=(-DNATIVE_READ_CASE=6); read_case=6; calls=7 ;;
        read-deferred) defines+=(-DNATIVE_READ_CASE=7); read_case=7; calls=7 ;;
        read-conflict) defines+=(-DNATIVE_READ_CASE=8); read_case=8; calls=7 ;;
        read-truncated) defines+=(-DNATIVE_READ_CASE=9); read_case=9; calls=7 ;;
        read-partial-error) defines+=(-DNATIVE_READ_CASE=10); read_case=10; calls=7 ;;
        read-eod) defines+=(-DNATIVE_READ_CASE=11); read_case=11 ;;
        read-empty) defines+=(-DNATIVE_READ_CASE=12); read_case=12 ;;
        read-short-legacy) defines+=(-DNO_RESIDUAL -DNATIVE_READ_CASE=1); read_case=1 ;;
        read-filemark-legacy) defines+=(-DNO_RESIDUAL -DNATIVE_READ_CASE=3); read_case=3 ;;
        control-host) defines+=(-DNATIVE_CONTROL_FAULT=1); control_exit=2 ;;
        control-target) defines+=(-DNATIVE_CONTROL_FAULT=2); control_exit=2 ;;
        control-deferred) defines+=(-DNATIVE_CONTROL_FAULT=3); control_exit=2 ;;
        control-unsupported) defines+=(-DNATIVE_CONTROL_FAULT=4); control_exit=2 ;;
        control-contradict) defines+=(-DNATIVE_CONTROL_FAULT=5); control_exit=2 ;;
        *) echo "Unknown native fixture: $variant" >&2; exit 1 ;;
    esac
    calls=$((calls + writer_calls))
    fixed_command='REM no fixed session requested'
    if test "$fixed_mode" = 1; then fixed_command="NTFIX $fixed_read $fixed_write $fixed_legacy > AUX"; fi
    writer_command='REM no native writer requested'
    if test "$write_case" -ge 0; then writer_command="NTWRITE $write_case $finish_fault $legacy_write > AUX"; fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' 'DEVICE=ASPITAP.SYS -w' 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    printf '%s\r\n' '@ECHO OFF' \
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'NAPIUT > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --status > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --native-status > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --native-status --refresh > AUX' \
        "IF ERRORLEVEL $((query_exit + 1)) GOTO FAILED" \
        "IF ERRORLEVEL $query_exit GOTO QUERYOK" 'GOTO FAILED' ':QUERYOK' \
        'OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --native-rewind > AUX' \
        "IF ERRORLEVEL $((control_exit + 1)) GOTO FAILED" \
        "IF ERRORLEVEL $control_exit GOTO CONTROLOK" 'GOTO FAILED' ':CONTROLOK' \
        'OPENTAPE --native-rewind --refresh > AUX' \
        'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --native-rewind --device RMT0 > AUX' \
        'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED' \
        'OPENTAPE --native-ready --device NRMT3 > AUX' \
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED' \
        "NTREAD $read_case > AUX" 'IF ERRORLEVEL 1 GOTO FAILED' \
        "$writer_command" 'IF ERRORLEVEL 1 GOTO FAILED' \
        "$fixed_command" 'IF ERRORLEVEL 1 GOTO FAILED' \
        'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'ECHO OPENTAPE NATIVE API PASSED > AUX' 'GOTO DONE' ':FAILED' \
        'ECHO OPENTAPE NATIVE API FAILED > AUX' ':DONE' 'QEXIT' > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NAPIUT.EXE" "$OUT/bin-$MAXLANG/NTREAD.EXE" "$OUT/bin-$MAXLANG/NTWRITE.EXE" "$OUT/bin-$MAXLANG/NTFIX.EXE" \
        "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" ::/
    mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/NRMT3
    set +e
    timeout 60 qemu-system-i386 -machine pc -m 16 -display none \
        -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a \
        -serial file:"$run_dir/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
    rc=$?
    set -e
    test "$rc" = 1 || { echo "QEMU failed in $variant: $rc" >&2; exit 1; }
    ! grep -Fq 'FAILED' "$run_dir/serial.log"
    grep -Fq 'OPENTAPE NATIVE API PASSED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed native record tests passed' "$run_dir/serial.log"
    if test "$variant" = legacy; then
        case "$MAXLANG" in
            US) grep -Fq 'Reply byte count unverified' "$run_dir/serial.log" ;;
            GR) grep -Fq 'Antwortlaenge durch alten ASPI-Manager' "$run_dir/serial.log" ;;
        esac
    fi
    if test "$write_case" -ge 0; then
        grep -Fq 'OpenTape installed native writer tests passed' "$run_dir/serial.log"
    fi
    if test "$fixed_mode" = 1; then
        grep -Fq 'OpenTape installed native fixed-block tests passed' "$run_dir/serial.log"
    fi
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'OpenTape native DOS API: %s %s passed\n' "$MAXLANG" "$variant"
done

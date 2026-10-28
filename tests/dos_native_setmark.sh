#!/bin/bash
# Classified native READ marks, using disposable DOS images and memory-only ASPI.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-setmark}
MAXLANG=${MAXLANG:-US}
SDK=${SETMARK_SDK:-$OPENASPI}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$SDK" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/NTSET.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal data descriptor descriptor-data legacy legacy-data descriptor-legacy noinfo data-noinfo unknown descriptor-unknown
    fixed fixed-data fixed-descriptor fixed-descriptor-data fixed-legacy fixed-legacy-data fixed-unknown
    host deferred contradict hard missing-mark conflict descriptor-host descriptor-deferred descriptor-hard descriptor-missing-mark descriptor-conflict
    fixed-host fixed-deferred fixed-hard fixed-missing-mark fixed-conflict filemark)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_SETMARK_TEST)
    fixed=0 combined=0 fault=0 unknown=0 calls=3
    case "$variant" in
        fixed*) fixed=1; defines+=(-DNATIVE_SETMARK_FIXED -DNATIVE_FIXED_LIMITS) ;;
    esac
    case "$variant" in
        *data*) combined=1; defines+=(-DNATIVE_SETMARK_DATA) ;;
    esac
    case "$variant" in
        *descriptor*) defines+=(-DNATIVE_SETMARK_DESCRIPTOR) ;;
    esac
    case "$variant" in
        *legacy*|*unknown*) defines+=(-DNO_RESIDUAL) ;;
    esac
    case "$variant" in
        *noinfo*) defines+=(-DNATIVE_SETMARK_NOINFO) ;;
        *unknown*) unknown=1; defines+=(-DNATIVE_SETMARK_NOINFO); calls=1 ;;
    esac
    case "$variant" in
        *host) fault=1 ;; *deferred) fault=2 ;; *contradict) fault=3 ;; *hard) fault=4 ;;
        *missing-mark) fault=5 ;; *conflict) fault=6 ;; filemark) fault=7 ;;
    esac
    if test "$fault" -gt 0; then
        defines+=(-DNATIVE_SETMARK_FAULT="$fault"); calls=1
        if test "$fault" = 7; then calls=2; fi
    fi
    if test "$SDK" != "$OPENASPI"; then calls=1; fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -DEXPECTED_WRITES=0 -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    driver='DEVICE=ASPITAP.SYS -w'
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' "$driver" 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    printf '%s\r\n' '@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        "NTSET $fixed $combined $fault $unknown > AUX" 'IF ERRORLEVEL 1 GOTO FAILED' \
        'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE SETMARK PASSED > AUX' \
        'GOTO DONE' ':FAILED' 'ECHO NATIVE SETMARK FAILED > AUX' ':DONE' 'QEXIT' > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" "$OUT/bin-$MAXLANG/NTSET.EXE" \
        "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" ::/
    set +e
    timeout 60 qemu-system-i386 -machine pc -m 16 -display none \
        -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a -serial file:"$run_dir/serial.log" \
        -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
    result=$?
    set -e
    test "$result" = 1
    ! grep -Fq 'NATIVE SETMARK FAILED' "$run_dir/serial.log"
    grep -Fq 'Installed native setmark distinction/archive gate passed' "$run_dir/serial.log"
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE SETMARK PASSED' "$run_dir/serial.log"
    printf 'Native setmark DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

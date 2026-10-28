#!/bin/bash
# Explicit owned setmark writes; disposable DOS images and memory-only ASPI.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-setwrite}
MAXLANG=${MAXLANG:-US}
SDK=${SETWRITE_SDK:-$OPENASPI}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$SDK" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/NTSETW.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy fixed fixed-legacy abandon abort fixed-abandon fixed-abort
    host unsupported deferred contradict target busy mark descriptor protect
    fixed-host fixed-unsupported fixed-deferred fixed-contradict fixed-target fixed-busy fixed-mark fixed-descriptor fixed-protect)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_WRITE_SETMARK_TEST)
    fixed=0 fault=0 abandon=0 calls=3 setmarks=2
    case "$variant" in fixed*) fixed=1; defines+=(-DNATIVE_FIXED_LIMITS) ;; esac
    case "$variant" in *legacy*) defines+=(-DNO_RESIDUAL) ;; esac
    case "$variant" in
        *abandon) abandon=1 ;; *abort) abandon=2 ;;
        *host) fault=1 ;; *unsupported) fault=2 ;; *deferred) fault=3 ;; *contradict) fault=4 ;;
        *target) fault=5 ;; *busy) fault=6 ;; *mark) fault=7 ;; *descriptor) fault=8 ;; *protect) fault=9 ;;
    esac
    if test "$fault" -gt 0; then defines+=(-DNATIVE_SETWRITE_FAULT="$fault"); fi
    if test "$fault" -gt 0 || test "$abandon" -gt 0; then calls=4; fi
    if test "$SDK" != "$OPENASPI"; then calls=1; setmarks=0; fi
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -DEXPECTED_WRITES=0 -DEXPECTED_MARKS=0 -DEXPECTED_FLUSHES=1 -DEXPECTED_SETMARKS="$setmarks" -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    driver='DEVICE=ASPITAP.SYS -w'
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' "$driver" 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    printf '%s\r\n' '@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        "NTSETW $fixed $fault $abandon > AUX" 'IF ERRORLEVEL 1 GOTO FAILED' \
        'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE SETWRITE PASSED > AUX' \
        'GOTO DONE' ':FAILED' 'ECHO NATIVE SETWRITE FAILED > AUX' ':DONE' 'QEXIT' > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" "$OUT/bin-$MAXLANG/NTSETW.EXE" \
        "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" ::/
    set +e
    timeout 60 qemu-system-i386 -machine pc -m 16 -display none \
        -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a -serial file:"$run_dir/serial.log" \
        -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
    result=$?
    set -e
    test "$result" = 1
    ! grep -Fq 'NATIVE SETWRITE FAILED' "$run_dir/serial.log"
    grep -Fq 'Installed native setmark write/ownership/no-replay tests passed' "$run_dir/serial.log"
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE SETWRITE PASSED' "$run_dir/serial.log"
    printf 'Native setwrite DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

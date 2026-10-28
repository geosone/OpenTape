#!/bin/bash
# Signed SPACE through the installed provider/SDK and localized CLI.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-extended-space-dos}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
make -C "$ROOT" -f dos.mk OPENASPI="$OPENASPI" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTXSPACE.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy seq-host seq-unsupported seq-deferred seq-contradict seq-mark seq-descriptor set-host set-unsupported set-deferred set-contradict set-mark set-descriptor seqset-host seqset-unsupported seqset-deferred seqset-contradict seqset-mark seqset-descriptor)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PROBE_TEST -DNATIVE_SPACE_TEST -DNATIVE_EXTENDED_SPACE_TEST)
    argument='' calls=19 cli_calls=15 writes=1 verb='--native-space-seq-filemarks'
    case "$variant" in
        normal) ;;
        legacy) defines+=(-DNO_RESIDUAL) ;;
        seq-*|set-*|seqset-*)
            case "$variant" in
                seq-*) op=14; code=2; verb='--native-space-seq-filemarks' ;;
                set-*) op=15; code=4; verb='--native-space-setmarks' ;;
                seqset-*) op=16; code=5; verb='--native-space-seq-setmarks' ;;
            esac
            case "$variant" in
                *-host) k=1; diagnostic=1 ;; *-unsupported) k=2; diagnostic=4 ;;
                *-deferred) k=3; diagnostic=2 ;; *-contradict) k=4; diagnostic=1 ;;
                *-mark) k=5; diagnostic=2 ;; *-descriptor) k=6; diagnostic=4 ;;
            esac
            defines+=(-DNATIVE_SPACE_FAULT="$k" -DNATIVE_EXTENDED_SPACE_CODE="$code")
            argument="$op $diagnostic"; calls=4; cli_calls=4; writes=0 ;;
        *) echo "Unknown extended SPACE fixture: $variant" >&2; exit 1 ;;
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
    lines=('@ECHO OFF' 'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' "NTXSPACE $argument > AUX"
        'IF ERRORLEVEL 1 GOTO FAILED' 'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
        'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED')
    for command in \
        '--native-space-records 8388608' '--native-space-filemarks -8388609' \
        '--native-space-records 999999999999' '--native-space-records -2147483648' \
        '--native-space-records nope' '--native-space-records +1' \
        '--native-space-records' '--native-eod 1' '--native-space-records 1 --refresh'; do
        lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
    done
    for verb_check in --native-space-seq-filemarks --native-space-setmarks --native-space-seq-setmarks; do
        for suffix in '' '8388608' '-8388609' '+1' '1 --refresh' '1 --device NRMT0 --device NRMT0'; do
            lines+=("OPENTAPE $verb_check $suffix > AUX" 'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED')
        done
    done
    lines+=('OPENTAPE --native-space-records 1 --device RMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO FAILED' 'IF NOT ERRORLEVEL 1 GOTO FAILED'
        'OPENTAPE --native-space-records 1 --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    if test -n "$argument"; then
        for command in "$verb 1" "$verb 1" '--native-status'; do
            lines+=("OPENTAPE $command > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        done
        lines+=('OPENTAPE --native-ready > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            'OPENTAPE --native-rewind > AUX' 'IF ERRORLEVEL 1 GOTO FAILED'
            "OPENTAPE $verb 1 > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
    else
        for verb in --native-space-seq-filemarks --native-space-setmarks --native-space-seq-setmarks; do
            for count in 1 -1 8388607 -8388608 0; do
                lines+=("OPENTAPE $verb $count > AUX" 'IF ERRORLEVEL 1 GOTO FAILED')
            done
        done
    fi
    lines+=('CLI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE EXTENDED SPACE CLI PASSED > AUX'
        'GOTO DONE' ':FAILED' 'ECHO NATIVE EXTENDED SPACE CLI FAILED > AUX' ':DONE' 'QEXIT')
    printf '%s\r\n' "${lines[@]}" > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$OUT/bin-$MAXLANG/NTXSPACE.EXE" \
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
    ! grep -Fq 'NATIVE EXTENDED SPACE CLI FAILED' "$run_dir/serial.log"
    grep -Fq 'OpenTape installed extended SPACE tests passed' "$run_dir/serial.log"
    grep -Fq 'NATIVE EXTENDED SPACE CLI PASSED' "$run_dir/serial.log"
    test "$(grep -Fc 'Native exact command counts passed' "$run_dir/serial.log")" = 2
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    printf 'Extended SPACE DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

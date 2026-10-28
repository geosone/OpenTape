#!/bin/bash
# QUILIB controls through real VGA/keyboard interaction and exact SCSI counters.
# Each run has a disposable DOS image and memory-only SCSIMGR$; no host tape.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/../OpenAspi}
OPENASPI=$(cd "$OPENASPI" && pwd)
OUT=${OUT:-$ROOT/build/native-controls}
MAXLANG=${MAXLANG:-US}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
mkdir -p "$OUT"
make -C "$OPENASPI" all > "$OUT/provider-build.log" 2>&1
SDK=${NATIVE_UI_SDK:-$OPENASPI}
make -C "$ROOT" -f dos.mk OPENASPI="$SDK" MAXLANG="$MAXLANG" \
    OUT="$OUT/obj-$MAXLANG" BINDIR="$OUT/bin-$MAXLANG" \
    "$OUT/bin-$MAXLANG/OPENTAPE.EXE" > "$OUT/build-$MAXLANG.log" 2>&1
variants=(normal legacy-ui host deferred unsupported unknown legacy)
if test "$#" -gt 0; then variants=("$@"); fi
for variant in "${variants[@]}"; do
    run_dir="$OUT/$MAXLANG-$variant"
    mkdir -p "$run_dir"
    defines=(-DNATIVE_PARTITIONS_TEST -DNATIVE_PROBE_TEST -DNATIVE_MEDIA_TEST -DNATIVE_POSITION_TEST -DNATIVE_LOCATE_TEST -DNATIVE_WIDE_TEST -DNATIVE_LOCK_TEST -DNATIVE_UI_TEST)
    calls=18 diagnostic=0 retensions=0
    case "$variant" in
        normal) calls=20; retensions=1 ;;
        alerts-normal) defines+=(-DNATIVE_ALERTS_TEST); calls=1 ;;
        alerts-all) defines+=(-DNATIVE_ALERTS_TEST -DNATIVE_ALERTS_KIND=1); calls=1 ;;
        alerts-clear) defines+=(-DNATIVE_ALERTS_TEST -DNATIVE_ALERTS_KIND=2); calls=1 ;;
        alerts-empty) defines+=(-DNATIVE_ALERTS_TEST -DNATIVE_ALERTS_KIND=3); calls=1 ;;
        alerts-legacy) defines+=(-DNATIVE_ALERTS_TEST -DNO_RESIDUAL); calls=1 ;;
        alerts-malformed) defines+=(-DNATIVE_ALERTS_TEST -DNATIVE_ALERTS_KIND=10); calls=1 ;;
        alerts-host) defines+=(-DNATIVE_ALERTS_TEST -DNATIVE_ALERTS_FAULT=1); calls=1 ;;
        space-ext-normal) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_EXTENDED_SPACE_TEST); calls=15 ;;
        space-ext-legacy) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_EXTENDED_SPACE_TEST -DNO_RESIDUAL); calls=15 ;;
        space-ext-host|space-ext-unsupported|space-ext-deferred|space-ext-contradict|space-ext-mark|space-ext-descriptor)
            case "$variant" in
                *-host) k=1; diagnostic=1 ;; *-unsupported) k=2; diagnostic=4 ;;
                *-deferred) k=3; diagnostic=2 ;; *-contradict) k=4; diagnostic=1 ;;
                *-mark) k=5; diagnostic=2 ;; *-descriptor) k=6; diagnostic=4 ;;
            esac
            defines+=(-DNATIVE_SPACE_TEST -DNATIVE_EXTENDED_SPACE_TEST -DNATIVE_EXTENDED_SPACE_CODE=4 -DNATIVE_SPACE_FAULT="$k")
            calls=5 ;;
        space-normal) defines+=(-DNATIVE_SPACE_TEST); calls=8 ;;
        space-legacy) defines+=(-DNATIVE_SPACE_TEST -DNO_RESIDUAL); calls=8 ;;
        space-host) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_SPACE_FAULT=1); calls=5; diagnostic=1 ;;
        space-unsupported) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_SPACE_FAULT=2); calls=5; diagnostic=4 ;;
        space-deferred) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_SPACE_FAULT=3); calls=5; diagnostic=2 ;;
        space-contradict) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_SPACE_FAULT=4); calls=5; diagnostic=1 ;;
        space-mark) defines+=(-DNATIVE_SPACE_TEST -DNATIVE_SPACE_FAULT=5); calls=5; diagnostic=2 ;;
        layout-legacy) defines+=(-DNO_RESIDUAL); calls=1 ;;
        layout-malformed) defines+=(-DNATIVE_PARTITIONS_KIND=3); calls=1 ;;
        layout-host) defines+=(-DNATIVE_PARTITIONS_FAULT=1); calls=1 ;;
        old-sdk) calls=0 ;;
        legacy-ui) calls=0 ;;
        host) defines+=(-DNATIVE_WIDE_FAULT=1); calls=6; diagnostic=1 ;;
        deferred) defines+=(-DNATIVE_WIDE_FAULT=3); calls=6; diagnostic=2 ;;
        unsupported) defines+=(-DNATIVE_WIDE_FAULT=5); calls=6; diagnostic=4 ;;
        unknown) defines+=(-DNATIVE_WIDE_CASE=1); calls=3 ;;
        legacy) defines+=(-DNO_RESIDUAL); calls=3 ;;
        *) echo "Unknown controls fixture: $variant" >&2; exit 1 ;;
    esac
    (cd "$OPENASPI" && build/tools/jwasm/jwasm -q -bin -0 -W4 -WX "${defines[@]}" \
        -Fo="$run_dir/MOCKASPI.SYS" tests/mockaspi.asm)
    nasm -f bin -o "$run_dir/QEXIT.COM" "$OPENASPI/../ahciaspi/tools/qexit.asm"
    nasm -f bin -DRESET=1 -o "$run_dir/RESET.COM" "$ROOT/tests/native_api_counts.asm"
    nasm -f bin -DEXPECTED_CALLS="$calls" -DEXPECTED_WRITES=0 -DEXPECTED_RETENSIONS="$retensions" \
        -o "$run_dir/COUNTS.COM" "$ROOT/tests/native_api_counts.asm"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$run_dir/boot.img"
    while IFS= read -r file; do
        file=${file#::/}
        case "${file^^}" in KERNEL.SYS|COMMAND.COM) ;; *) mdel -i "$run_dir/boot.img" "::/$file" ;; esac
    done < <(mdir -b -i "$run_dir/boot.img" ::/)
    printf '%s\r\n' 'DEVICE=MOCKASPI.SYS' 'DEVICE=ASPITAP.SYS -w' 'SHELL=COMMAND.COM /P' > "$run_dir/CONFIG.SYS"
    ui_command='OPENTAPE --native --ui > AUX'
    if test "$variant" = legacy-ui; then ui_command='OPENTAPE --ui > AUX'; fi
    sdk_checks=()
    if test "$variant" = old-sdk; then
        for verb in --native-space-seq-filemarks --native-space-setmarks --native-space-seq-setmarks; do
            sdk_checks+=("OPENTAPE $verb 1 > AUX" 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
        done
        sdk_checks+=('OPENTAPE --native-alerts --allow-clear > AUX' 'IF ERRORLEVEL 3 GOTO FAILED' 'IF NOT ERRORLEVEL 2 GOTO FAILED')
    fi
    printf '%s\r\n' '@ECHO OFF'  'RESET > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        "$ui_command" 'IF ERRORLEVEL 1 GOTO FAILED' "${sdk_checks[@]}" \
        'COUNTS > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' 'ECHO NATIVE CONTROLS PASSED > AUX' \
        'GOTO DONE' ':FAILED' 'ECHO NATIVE CONTROLS FAILED > AUX' ':DONE' 'QEXIT' > "$run_dir/AUTOEXEC.BAT"
    mcopy -o -i "$run_dir/boot.img" "$run_dir/CONFIG.SYS" "$run_dir/AUTOEXEC.BAT" \
        "$run_dir/MOCKASPI.SYS" "$OPENASPI/build/dos/ASPITAP.SYS" \
        "$OUT/bin-$MAXLANG/OPENTAPE.EXE" "$run_dir/RESET.COM" "$run_dir/COUNTS.COM" "$run_dir/QEXIT.COM" ::/
    mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/NRMT3
    set +e
    (OUT="$run_dir"; source "$ROOT/tests/native_controls_monitor.sh") | \
        timeout 180 qemu-system-i386 -machine pc -m 16 -display none -monitor stdio \
        -drive if=floppy,format=raw,file="$run_dir/boot.img" -boot a \
        -serial file:"$run_dir/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$run_dir/process.log" 2>&1
    monitor_status=("${PIPESTATUS[@]}")
    set -e
    test "${monitor_status[0]}" = 0
    test "${monitor_status[1]}" = 1
    ! grep -Fq 'NATIVE CONTROLS FAILED' "$run_dir/serial.log"
    grep -Fq 'NATIVE CONTROLS PASSED' "$run_dir/serial.log"
    grep -Fq 'Native exact command counts passed' "$run_dir/serial.log"
    mtype -i "$run_dir/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
    if test "$variant" = space-normal || test "$variant" = space-legacy; then
        python3 "$ROOT/tests/native_space_colors.py" "$run_dir"
    fi
    if test "$variant" = normal; then
        python3 "$ROOT/tests/native_controls_colors.py" "$run_dir"
    fi
    printf 'Native controls DOS: %s %s passed\n' "$MAXLANG" "$variant"
done

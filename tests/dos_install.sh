#!/bin/bash
# The OpenTape package (make dist) on a floppy, installed silently with its
# GDI INSTALL.EXE into a fresh disposable DOS hard disk (SvarDOS) with the
# proposed options, installed again over itself, then removed by INSTALL /U
# from the target directory (R and Enter through the QEMU monitor); one QEMU
# boot per step. INSTALL /CHECK must
# accept the package, the files must match it, PathAppend= must put the
# directory into the PATH once (also after the second installation), /U must
# take it out again and remove the files, and the disk must stay consistent.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-$ROOT/build/install-dos}
SVARDOS_ZIP=${SVARDOS_ZIP:-$HOME/Iso/Dos/svardos-20250427-floppy-1.44M.zip}
languages=(US GR)
if test "$#" -gt 0; then languages=("$@"); fi
mkdir -p "$OUT"
for language in "${languages[@]}"; do
    make -C "$ROOT" MAXLANG="$language" dist > "$OUT/dist-$language.log" 2>&1
    dist="$ROOT/build/$language/dist"
    work="$OUT/$language"
    rm -rf "$work"
    mkdir -p "$work"
    unzip -p "$SVARDOS_ZIP" svdos-1.44M-disk-1.img > "$work/boot.img"
    mkfs.fat -C -n OPENTAPE "$work/package.img" 1440 > /dev/null
    mcopy -i "$work/package.img" "$dist"/* ::/
    truncate -s 32M "$work/storage.img"
    printf 'start=63,type=6\n' | sfdisk -q "$work/storage.img"
    mkfs.fat -F16 -h63 --offset 63 "$work/storage.img" $(((32*2048-63)/2)) > /dev/null
    printf '%s\r\n' 'LASTDRIVE=Z' 'SHELL=COMMAND.COM /P' > "$work/CONFIG.SYS"
    printf '%s\r\n' '@ECHO OFF' 'IF EXIST C:\STEP2 GOTO STEP3' 'IF EXIST C:\STEP1 GOTO STEP2' \
        'ECHO REM scratch test > C:\CONFIG.SYS' 'ECHO REM scratch test > C:\AUTOEXEC.BAT' \
        'B:\INSTALL /CHECK B:\INSTALL.INI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'B:\INSTALL /B:C: /S B:\INSTALL.INI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'ECHO === AUTO1 > AUX' 'TYPE C:\AUTOEXEC.BAT > AUX' 'ECHO === END1 > AUX' \
        'ECHO X > C:\STEP1' 'ECHO STEP1 DONE > AUX' 'QEXIT' ':STEP2' \
        'B:\INSTALL /B:C: /S B:\INSTALL.INI > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'ECHO === AUTO2 > AUX' 'TYPE C:\AUTOEXEC.BAT > AUX' 'ECHO === END2 > AUX' \
        'ECHO X > C:\STEP2' 'ECHO STEP2 DONE > AUX' 'QEXIT' ':STEP3' \
        'C:\OPENTAPE\INSTALL.EXE /U /B:C: > AUX' 'IF ERRORLEVEL 1 GOTO FAILED' \
        'IF EXIST C:\OPENTAPE\OPENTAPE.EXE GOTO FAILED' \
        'ECHO === AUTO3 > AUX' 'TYPE C:\AUTOEXEC.BAT > AUX' 'ECHO === END3 > AUX' \
        'ECHO INSTALL OPENTAPE PASSED > AUX' 'GOTO DONE' ':FAILED' \
        'ECHO INSTALL OPENTAPE FAILED > AUX' ':DONE' 'QEXIT' > "$work/AUTOEXEC.BAT"
    printf '\xba\xf4\x00\xb0\x00\xee\xeb\xfe' > "$work/QEXIT.COM"
    mcopy -o -i "$work/boot.img" "$work/CONFIG.SYS" "$work/AUTOEXEC.BAT" "$work/QEXIT.COM" ::/
    fail=0
    for step in 1 2 3; do
        # INSTALL /U asks before it removes anything and reports the result:
        # R and Enter through the monitor, as GDI's own uninstall test does
        keys() {
            if test "$step" = 3; then
                sleep 10; echo "screendump $work/uninstall1.ppm"; sleep 1
                echo 'sendkey r'; sleep 3; echo "screendump $work/uninstall2.ppm"
                echo 'sendkey ret'
            fi
            for _ in $(seq 1 240); do
                grep -Eq 'DONE|PASSED|FAILED' "$work/serial.log" 2>/dev/null && break
                sleep 0.5
            done
            sleep 1
        }
        rm -f "$work/serial.log"
        set +e
        keys | timeout "${TIMEOUT:-120}" qemu-system-i386 -machine pc -m 32 -display none \
            -monitor stdio \
            -drive "if=floppy,index=0,format=raw,file=$work/boot.img" -boot a \
            -drive "if=floppy,index=1,format=raw,readonly=on,file=$work/package.img" \
            -drive "if=ide,format=raw,file=$work/storage.img" \
            -serial "file:$work/serial.log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
            > "$work/process.log" 2>&1
        rc=${PIPESTATUS[1]}
        set -e
        tr -d '\0' < "$work/serial.log" >> "$work/serial.txt"
        test "$rc" = 1 || { cat "$work/serial.txt"; echo "qemu rc $rc in step $step"; exit 1; }
        if test "$step" = 1; then
            for file in OPENTAPE.EXE OTAPEWIN.EXE README.TXT LICENSE.TXT INSTALL.EXE INSTALL.INI; do
                mcopy -o -i "$work/storage.img@@32256" "::/OPENTAPE/$file" "$work/$file" 2>/dev/null ||
                    { echo "missing $file"; fail=1; continue; }
                cmp -s "$work/$file" "$dist/$file" || { echo "differs: $file"; fail=1; }
            done
        fi
    done
    grep -Fq 'STEP1 DONE' "$work/serial.txt" || fail=1
    grep -Fq 'STEP2 DONE' "$work/serial.txt" || fail=1
    grep -Fq 'INSTALL OPENTAPE PASSED' "$work/serial.txt" || fail=1
    for n in 1 2 3; do
        sed -n "/=== AUTO$n/,/=== END$n/p" "$work/serial.txt" | tr -d '\r' > "$work/auto$n.txt"
    done
    test "$(grep -ic 'C:\\OPENTAPE' "$work/auto1.txt")" = 1 || { echo "PATH not added once"; fail=1; }
    test "$(grep -ic 'C:\\OPENTAPE' "$work/auto2.txt")" = 1 || { echo "PATH not once after reinstall"; fail=1; }
    ! grep -iq 'OPENTAPE' "$work/auto3.txt" || { echo "PATH not removed by /U"; fail=1; }
    grep -Fxq 'REM scratch test ' "$work/auto3.txt" || grep -Fxq 'REM scratch test' "$work/auto3.txt" ||
        { echo "AUTOEXEC.BAT lost its own line"; fail=1; }
    ! mtype -i "$work/storage.img@@32256" ::/GDI.INI 2>/dev/null | grep -iq 'OPENTAPE' ||
        { echo "GDI.INI still lists OpenTape"; fail=1; }
    if mdir -i "$work/storage.img@@32256" ::/OPENTAPE >/dev/null 2>&1; then
        echo "C:\\OPENTAPE left after /U"; fail=1
    fi
    dd if="$work/storage.img" of="$work/partition.img" bs=512 skip=63 status=none
    fsck.fat -n "$work/partition.img" > "$work/fsck.log" 2>&1 || { echo "fsck.fat found errors"; fail=1; }
    if test "$fail" = 0; then
        echo "OpenTape DOS install $language passed"
    else
        cat "$work/serial.txt"
        exit 1
    fi
done

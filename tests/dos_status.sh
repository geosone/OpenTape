#!/bin/bash
# Read-only application smoke test using a prepared OpenASPI dc390 boot image.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/deps/OpenASPI}
MAXLANG=${MAXLANG:-US}
OUT="$ROOT/build/$MAXLANG/dos-status"
read_test=${OPENTAPE_READ_TEST:-0}
native_test=${OPENTAPE_NATIVE_TEST:-0}
case "$read_test" in
    0) ;;
    1) OUT="$ROOT/build/$MAXLANG/dos-read" ;;
    *) echo 'OPENTAPE_READ_TEST must be 0 or 1' >&2; exit 1 ;;
esac
case "$native_test" in
    0) ;;
    1)
        test "$read_test" = 0 || { echo 'Select only one data test mode' >&2; exit 1; }
        OUT="$ROOT/build/$MAXLANG/dos-native"
        ;;
    *) echo 'OPENTAPE_NATIVE_TEST must be 0 or 1' >&2; exit 1 ;;
esac
QEMU_BIN=${QEMU_BIN:-qemu-system-i386}
JWASM=${JWASM:-$ROOT/deps/Open386MAX/linux/bin/jwasm}
: "${DOS_BOOT_IMAGE:?Select a prepared OpenASPI dc390/mhVTL boot.img with DELAY.SYS}"
: "${MHVTL_TAPE:?Select the mhVTL generic tape device}"
: "${MHVTL_VERIFY_CHANGER:?Select the verification changer}"
: "${MHVTL_VERIFY_DRIVE:?Select the zero-based drive index}"
: "${MHVTL_SCRATCH_BARCODE:?Select the loaded virtual cartridge}"
case "$MAXLANG" in US|GR) ;; *) echo 'MAXLANG must be US or GR' >&2; exit 1 ;; esac
verify_tape() {
    bash "$OPENASPI/tests/mhvtl_write_guard.sh" "$MHVTL_TAPE" \
        "$MHVTL_VERIFY_CHANGER" "$MHVTL_VERIFY_DRIVE" "$MHVTL_SCRATCH_BARCODE"
}
verify_tape
test -f "$DOS_BOOT_IMAGE"
make -C "$ROOT" MAXLANG="$MAXLANG" all >/dev/null
mkdir -p "$OUT"
cp "$DOS_BOOT_IMAGE" "$OUT/boot.img"
cp "$ROOT/build/$MAXLANG/dos/OPENTAPE.EXE" "$OUT/OPENTAPE.EXE"
"$JWASM" -q -bin -0 -W4 -WX \
    -Fo="$OUT/QEXIT.COM" "$OPENASPI/tests/qexit.asm"
read_lines=()
if test "$read_test" = 1; then
    read_lines=(
        'OPENTAPE --read-test --device NMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --read-test --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --read-test > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'ECHO OpenTape DOS read passed > AUX'
    )
fi
native_lines=()
if test "$native_test" = 1; then
    native_lines=(
        'OPENTAPE --scan > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'ECHO OpenTape DOS non-native rejection passed > AUX'
    )
fi
lines=(
    '@ECHO OFF'
    'NATTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'PAYTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'OPENTAPE --help > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'OPENTAPE --status > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'OPENTAPE --status --device NRMT0 > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'OPENTAPE --status --device RMT0 > AUX'
    'IF ERRORLEVEL 2 GOTO failed'
    'IF NOT ERRORLEVEL 1 GOTO failed'
    'OPENTAPE --status --device NRMT2 > AUX'
    'IF ERRORLEVEL 3 GOTO failed'
    'IF NOT ERRORLEVEL 2 GOTO failed'
    'OPENTAPE --status --device NRMT3 > AUX'
    'IF ERRORLEVEL 3 GOTO failed'
    'IF NOT ERRORLEVEL 2 GOTO failed'
    "${read_lines[@]}"
    "${native_lines[@]}"
    'ECHO OpenTape DOS status passed > AUX'
    'GOTO finished'
    ':failed'
    'ECHO OpenTape DOS status FAILED > AUX'
    ':finished'
    'QEXIT'
)
printf '%s\r\n' "${lines[@]}" > "$OUT/AUTOEXEC.BAT"
mcopy -o -i "$OUT/boot.img" "$OUT/OPENTAPE.EXE" "$OUT/QEXIT.COM" \
    "$OUT/AUTOEXEC.BAT" "$ROOT/build/$MAXLANG/dos/NATTEST.EXE" \
    "$ROOT/build/$MAXLANG/dos/PAYTEST.EXE" ::/
# A regular file with a device-like name must never be accepted as a tape.
mcopy -o -i "$OUT/boot.img" "$ROOT/LICENSE" ::/NRMT3
address=$(basename "$(readlink -f "/sys/class/scsi_generic/$(basename "$MHVTL_TAPE")/device")")
IFS=: read -r host channel target lun <<< "$address"
guest_target=$((10#$target % 8))
verify_tape
if test "$read_test" = 1 || test "$native_test" = 1; then
    # Explicitly gated, host-side rewind of the approved virtual fixture only.
    # The application itself never rewinds. No media data is written here.
    sg_raw "$MHVTL_TAPE" 01 00 00 00 00 00 > "$OUT/rewind.log" 2>&1
fi
: > "$OUT/serial.log"
mkfifo "$OUT/monitor.fifo"
{
    sleep 3
    printf '%s\n' \
        "drive_add 0 if=none,id=tape,format=raw,file=$MHVTL_TAPE" \
        "device_add scsi-generic,drive=tape,bus=scsi0.0,scsi-id=$guest_target,lun=$lun"
} > "$OUT/monitor.fifo" &
writer=$!
trap 'kill "$writer" 2>/dev/null || true; rm -f -- "$OUT/monitor.fifo"' EXIT
set +e
timeout "${TIMEOUT:-45}" "$QEMU_BIN" -machine pc -m 16 -display none -vga std \
    -monitor stdio -drive if=floppy,format=raw,file="$OUT/boot.img" -boot a \
    -serial file:"$OUT/serial.log" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 -device dc390,id=scsi0 \
    < "$OUT/monitor.fifo" > "$OUT/qemu.log" 2>&1
rc=$?
set -e
wait "$writer"
trap 'rm -f -- "$OUT/monitor.fifo"' EXIT
if test "$rc" = 124; then echo 'DOS status test timed out' >&2; exit 1; fi
if test "$rc" != 1; then echo "Unexpected QEMU exit: $rc" >&2; exit 1; fi
tr -d '\r' < "$OUT/serial.log"
grep -q 'OpenTape DOS status passed' "$OUT/serial.log"
grep -q 'OpenTape native framing tests passed' "$OUT/serial.log"
grep -q 'OpenTape typed payload tests passed' "$OUT/serial.log"
grep -q "NRMT0 ASPI 00:0$guest_target:0$lun" "$OUT/serial.log"
! grep -q 'FAILED' "$OUT/serial.log"
if test "$read_test" = 1; then
    grep -q 'OpenTape DOS read passed' "$OUT/serial.log"
    grep -Eq '(Filemark|Dateimarke): 3 (blocks|Bloecke), 3072 (bytes|Byte)' "$OUT/serial.log"
    grep -q 'Block 0: 1024 .*CRC32=227BE3D1' "$OUT/serial.log"
    grep -q 'Block 1: 1024 .*CRC32=76A5CF1D' "$OUT/serial.log"
    grep -q 'Block 2: 1024 .*CRC32=4D13C3E7' "$OUT/serial.log"
fi
if test "$native_test" = 1; then
    grep -q 'OpenTape DOS non-native rejection passed' "$OUT/serial.log"
    grep -Eq '(Format error|Formatfehler) -2 ' "$OUT/serial.log"
fi
mtype -i "$OUT/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
echo "OpenTape $MAXLANG DOS smoke passed (read: $read_test, native rejection: $native_test)"

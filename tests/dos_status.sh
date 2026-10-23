#!/bin/bash
# Application smoke tests; only explicitly authorized tape mode writes media.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENASPI=${OPENASPI:-$ROOT/deps/OpenASPI}
MAXLANG=${MAXLANG:-US}
OUT="$ROOT/build/$MAXLANG/dos-status"
read_test=${OPENTAPE_READ_TEST:-0}
native_test=${OPENTAPE_NATIVE_TEST:-0}
tape_test=${OPENTAPE_TAPE_TEST:-none}
tree_large=${OPENTAPE_TREE_LARGE:-0}
ui_export=${OPENTAPE_UI_EXPORT:-0}
case "$ui_export" in 0|1) ;; *) exit 1 ;; esac
case "$tree_large" in 0|1) ;; *) exit 1 ;; esac
case "$read_test" in
    0) ;;
    1) OUT="$ROOT/build/$MAXLANG/dos-read" ;;
    *) echo 'OPENTAPE_READ_TEST must be 0 or 1' >&2; exit 1 ;;
esac
case "$tape_test" in
    none) ;;
    write|verify|cli-verify|file-write|file-read|file-verify|tree-write|tree-read|tree-verify|tree-limited|tree-restore|tree-replace|tree-list|tree-catalog|tree-filter|tree-preview|tree-readback|tree-compare|tree-devices|tree-ui)
        test "$read_test" = 0 && test "$native_test" = 0 || exit 1
        OUT="$ROOT/build/$MAXLANG/dos-tape-$tape_test"
        ;;
    *) echo 'Invalid OPENTAPE_TAPE_TEST mode' >&2; exit 1 ;;
esac
case "$native_test" in
    0) ;;
    1)
        test "$read_test" = 0 || { echo 'Select only one data test mode' >&2; exit 1; }
        OUT="$ROOT/build/$MAXLANG/dos-native"
        ;;
    *) echo 'OPENTAPE_NATIVE_TEST must be 0 or 1' >&2; exit 1 ;;
esac
if test "$tree_large" = 1; then
    [[ "$tape_test" = tree-* ]] || exit 1
    OUT="$OUT-large"
fi
if test "$ui_export" = 1; then
    test "$tape_test" = tree-ui && test "$tree_large" = 1 || exit 1
    OUT="$OUT-export"
fi
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
verify_writable() {
    test "${MHVTL_ALLOW_OVERWRITE:-}" = OA2101L8 &&
        test "$MHVTL_SCRATCH_BARCODE" = OA2101L8 || {
        echo 'Writing requires MHVTL_ALLOW_OVERWRITE=OA2101L8 and that exact scratch cartridge' >&2
        exit 1
    }
    local header
    local -a bytes
    header=$(LC_ALL=C sg_modes --six --dbd --maxlen=4 --raw "$MHVTL_TAPE" | od -An -tu1)
    read -r -a bytes <<< "$header"
    if test "${#bytes[@]}" != 4 || (( bytes[0] < 3 || (bytes[2] & 128) != 0 )); then
        echo 'Scratch tape does not report writable media' >&2
        exit 1
    fi
}
verify_tape
if [[ "$tape_test" = write || "$tape_test" = *-write ]]; then verify_writable; fi
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
        'OPENTAPE --verify > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'ECHO OpenTape DOS non-native rejection passed > AUX'
    )
fi
tape_lines=()
if test "$tape_test" = write; then
    tape_lines=(
        'TAPETEST --write --device NRMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'TAPETEST --write --overwrite --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = verify; then
    tape_lines=(
        'TAPETEST --verify --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = cli-verify; then
    tape_lines=(
        'OPENTAPE --verify --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = file-write; then
    tape_lines=(
        'OPENTAPE --backup-file SOURCE.BIN --as FILE.BIN --device NRMT0 --volume FILETEST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --backup-file MISSING.BIN --as FILE.BIN --device NRMT0 --overwrite --volume FILETEST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --backup-file SOURCE.BIN --as FILE.BIN --device NRMT0 --overwrite --volume FILETEST > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = file-read; then
    tape_lines=('FILEREAD > AUX' 'IF ERRORLEVEL 1 GOTO failed')
elif test "$tape_test" = file-verify; then
    tape_lines=('OPENTAPE --verify --device NRMT0 > AUX' 'IF ERRORLEVEL 1 GOTO failed')
elif test "$tape_test" = tree-write; then
    tape_lines=(
        'OPENTAPE --backup-tree MISSING --as ROOT --device NRMT0 --overwrite --volume TREE > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --backup-tree SRC --as ROOT --device NRMT0 --overwrite --volume TREE > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-read; then
    tape_lines=('TREEREAD > AUX' 'IF ERRORLEVEL 1 GOTO failed')
    if test "$tree_large" = 1; then tape_lines=('TREEREAD --large > AUX' 'IF ERRORLEVEL 1 GOTO failed'); fi
elif test "$tape_test" = tree-verify; then
    tape_lines=('OPENTAPE --verify --all --device NRMT0 > AUX' 'IF ERRORLEVEL 1 GOTO failed')
elif test "$tape_test" = tree-restore; then
    tape_lines=(
        'OPENTAPE --restore --device RMT0 --to NEVER > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to SRC > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to EXISTS > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'RSTCMP > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'RSTCMP > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-replace; then
    tape_lines=(
        'OPENTAPE --restore --device NRMT0 --to RESTORED --replace ROOT/*.BIN > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED --replace ROOT/DATA.BIN --replace ROOT/SUB/DATA.BIN > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'RSTCMP --preserve-zero > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-list; then
    tape_lines=(
        'OPENTAPE --restore --device NRMT0 --to RESTORED --replace-list BAD.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED --replace ROOT/DATA.BIN --replace-list DUP.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED --replace-list SELECT.LST --replace ROOT/DATA.BIN > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'RSTCMP --preserve-zero > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-ui; then
    test "$tree_large" = 1 || { echo 'UI fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'OPENTAPE --ui --device RMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --device NRT0$ > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --selection-out A.LST --selection-out B.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --selection-out > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --selection-out 12345678901234567890123456789012345678901 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --device nrst0$ > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --ui --device NRMT2 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
    if test "$ui_export" = 1; then
        for ui_index in "${!tape_lines[@]}"; do
            if test "${tape_lines[$ui_index]}" = 'OPENTAPE --ui --device NRMT2 > AUX'; then
                tape_lines[$ui_index]='OPENTAPE --ui --selection-out B:\PICKED.LST --device NRMT2 > AUX'
            fi
        done
    fi
elif test "$tape_test" = tree-devices; then
    test "$tree_large" = 1 || { echo 'Device enumeration fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'OPENTAPE --devices --letter MM > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --devices --letter / > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --devices --dollar --dollar > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --devices --letter M --letter M > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --devices --device NRMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --devices > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --devices --letter s --dollar > AUX'
        'IF ERRORLEVEL 4 GOTO failed'
        'IF NOT ERRORLEVEL 3 GOTO failed'
        'OPENTAPE --devices --dollar --letter R > AUX'
        'IF ERRORLEVEL 4 GOTO failed'
        'IF NOT ERRORLEVEL 3 GOTO failed'
        'IF EXIST OALOOK.TXT GOTO failed'
        'REN NRMT3 OALOOK.TXT'
        'IF EXIST NRMT3 GOTO failed'
        'IF NOT EXIST OALOOK.TXT GOTO failed'
        'OPENTAPE --devices --letter m > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'REN OALOOK.TXT NRMT3'
        'IF EXIST OALOOK.TXT GOTO failed'
        'IF NOT EXIST NRMT3 GOTO failed'
        'OPENTAPE --verify --all --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-compare; then
    test "$tree_large" = 1 || { echo 'Comparison fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'OPENTAPE --compare --device NRMT0 --to NEVER > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED --replace ROOT/DATA.BIN > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED --dry-run > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED --only-list EMPTY.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED --only-list TWO.LST > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
    for compare_target in BADCRC SHORT LONG ISDIR; do
        tape_lines+=(
            'OPENTAPE --rewind > AUX'
            'IF ERRORLEVEL 1 GOTO failed'
            "OPENTAPE --compare --device NRMT0 --to $compare_target --only ROOT/DATA.BIN > AUX"
            'IF ERRORLEVEL 3 GOTO failed'
            'IF NOT ERRORLEVEL 2 GOTO failed'
        )
    done
    tape_lines+=(
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to MISSINGF --only ROOT/SUB/F39.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to BROKEN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --compare --device NRMT0 --to RESTORED --only MISSING.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST NEVER GOTO failed'
    )
elif test "$tape_test" = tree-readback; then
    test "$tree_large" = 1 || { echo 'Readback fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'RBTEST > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RESTORED > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-preview; then
    test "$tree_large" = 1 || { echo 'Preview fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'OPENTAPE --restore --device NRMT0 --to NEVER --dry-run --dry-run > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEVER --dry-run --only > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to PICKED --dry-run > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to MISSING\ROOT --dry-run > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NRMT3\CHILD --dry-run > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEVER --dry-run --only-list EMPTY.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to RECOVER --dry-run --replace ROOT/DATA.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT3 --to NEVER --dry-run > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to DRYROOT --dry-run > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'IF EXIST DRYROOT GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to PICKED --dry-run --only-list ONLY.LST --only ROOT/SUB/ZERO.BIN --replace-list ONLY.LST > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NOHIT --dry-run --only MISSING.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST NOHIT GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to BLOCKED --dry-run --replace ROOT/DATA.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST NEVER GOTO failed'
    )
elif test "$tape_test" = tree-filter; then
    test "$tree_large" = 1 || { echo 'Filter fixture requires large tree' >&2; exit 1; }
    tape_lines=(
        'OPENTAPE --restore --device NRMT0 --to NEVER --only-list EMPTY.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEVER --only-list BADONLY.LST > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEVER --only-list ONLY.LST --only root/sub/f39.bin > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEVER --only-list ONLY.LST --replace ROOT/DATA.BIN > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'IF EXIST NEVER GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to PICKED --only-list ONLY.LST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NEWPICK --only-list ONLY.LST --only ROOT/SUB/ZERO.BIN > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'IF EXIST NEWPICK\ROOT\DATA.BIN GOTO failed'
        'IF EXIST NEWPICK\ROOT\SUB\DATA.BIN GOTO failed'
        'IF EXIST NEWPICK\ROOT\EMPTY GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to PICKED --only-list ONLY.LST --only ROOT/SUB/ZERO.BIN --replace ROOT/SUB/F39.BIN > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'IF EXIST PICKED\ROOT\DATA.BIN GOTO failed'
        'IF EXIST PICKED\ROOT\EMPTY GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --restore --device NRMT0 --to NOHIT --only MISSING.BIN > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST NOHIT\ROOT GOTO failed'
    )
elif test "$tape_test" = tree-catalog; then
    tape_lines=(
        'OPENTAPE --rewind --all > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind --device NMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind --device NRT0$ > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind --device RMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --all > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --list --device RMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --list --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out OT$LIST.TMP > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out CON > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out SRC > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out MISSING\FILES.LST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --list --selection-out SRC\EMPTY\FILES.LST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST SRC\EMPTY\FILES.LST GOTO failed'
        'OPENTAPE --list --device NRMT3 --selection-out BADOUT.LST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'IF EXIST BADOUT.LST GOTO failed'
        'IF EXIST OT$LIST.TMP GOTO failed'
        'OPENTAPE --list --device NRMT0 --selection-out FILES.LST > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'IF EXIST OT$LIST.TMP GOTO failed'
        'OPENTAPE --list --selection-out FILES.LST > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
    )
    if test "$MAXLANG" = GR; then
        tape_lines[${#tape_lines[@]}-6]='OPENTAPE --list --selection-out FILES.LST > AUX'
    fi
    tape_lines+=(
        'OPENTAPE --rewind --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all --device NRMT0 > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --rewind > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all > AUX'
        'IF ERRORLEVEL 1 GOTO failed'
    )
elif test "$tape_test" = tree-limited; then
    tape_lines=(
        'OPENTAPE --scan --all > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all --device RMT0 > AUX'
        'IF ERRORLEVEL 2 GOTO failed'
        'IF NOT ERRORLEVEL 1 GOTO failed'
        'OPENTAPE --verify --all --device NRMT3 > AUX'
        'IF ERRORLEVEL 3 GOTO failed'
        'IF NOT ERRORLEVEL 2 GOTO failed'
        'OPENTAPE --verify --device NRMT0 > AUX'
        'IF ERRORLEVEL 4 GOTO failed'
        'IF NOT ERRORLEVEL 3 GOTO failed'
    )
fi
lines=(
    '@ECHO OFF'
    'NATTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'PAYTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'VERTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'WRITEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'OUTTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'SRCTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'PLANTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'STRMTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'RESTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'SELTEST > AUX'
    'IF ERRORLEVEL 1 GOTO failed'
    'PRBTEST > AUX'
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
    "${tape_lines[@]}"
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
    "$ROOT/build/$MAXLANG/dos/PAYTEST.EXE" "$ROOT/build/$MAXLANG/dos/VERTEST.EXE" \
    "$ROOT/build/$MAXLANG/dos/WRITEST.EXE" "$ROOT/build/$MAXLANG/dos/OUTTEST.EXE" \
    "$ROOT/build/$MAXLANG/dos/TAPETEST.EXE" "$ROOT/build/$MAXLANG/dos/SRCTEST.EXE" \
    "$ROOT/build/$MAXLANG/dos/FILEREAD.EXE" "$ROOT/build/$MAXLANG/dos/TREEREAD.EXE" \
    "$ROOT/build/$MAXLANG/dos/PLANTEST.EXE" "$ROOT/build/$MAXLANG/dos/STRMTEST.EXE" \
    "$ROOT/build/$MAXLANG/dos/RESTEST.EXE" "$ROOT/build/$MAXLANG/dos/RSTCMP.EXE" \
    "$ROOT/build/$MAXLANG/dos/SELTEST.EXE" "$ROOT/build/$MAXLANG/dos/PRBTEST.EXE" ::/
if test "$tape_test" = tree-readback; then
    mcopy -i "$OUT/boot.img" "$ROOT/build/$MAXLANG/dos/RBTEST.EXE" ::/
fi
mcopy -m -o -i "$OUT/boot.img" "$ROOT/build/$MAXLANG/dos/TAPETEST.EXE" ::/SOURCE.BIN
if [[ "$tape_test" = tree-* ]]; then
    mmd -i "$OUT/boot.img" ::/SRC ::/SRC/EMPTY ::/SRC/SUB
    mcopy -m -i "$OUT/boot.img" "$ROOT/build/$MAXLANG/dos/TAPETEST.EXE" ::/SRC/DATA.BIN
    mcopy -m -i "$OUT/boot.img" "$ROOT/build/$MAXLANG/dos/OUTTEST.EXE" ::/SRC/SUB/DATA.BIN
    mattrib -i "$OUT/boot.img" +r +h +s ::/SRC/SUB/DATA.BIN
    mcopy -m -i "$OUT/boot.img" "$ROOT/tests/fixtures/EMPTY.BIN" ::/SRC/SUB/ZERO.BIN
    if test "$tree_large" = 1; then
        for tree_index in $(seq -w 0 39); do
            mcopy -m -i "$OUT/boot.img" "$ROOT/tests/fixtures/EMPTY.BIN" "::/SRC/SUB/F$tree_index.BIN"
        done
    fi
fi
if test "$tape_test" = tree-restore; then mmd -i "$OUT/boot.img" ::/EXISTS; fi
if test "$tape_test" = tree-compare; then
    compare_image="$ROOT/build/$MAXLANG/dos-tape-tree-readback-large/boot.img"
    test -r "$compare_image" || { echo 'Comparison requires the readback restore fixture' >&2; exit 1; }
    mcopy -s -m -i "$compare_image" ::/RESTORED "$OUT/"
    mcopy -s -m -i "$OUT/boot.img" "$OUT/RESTORED" ::/
    for compare_target in BADCRC SHORT LONG ISDIR BROKEN; do
        mmd -i "$OUT/boot.img" "::/$compare_target" "::/$compare_target/ROOT"
    done
    mmd -i "$OUT/boot.img" ::/ISDIR/ROOT/DATA.BIN ::/MISSINGF ::/MISSINGF/ROOT ::/MISSINGF/ROOT/SUB
    compare_source="$OUT/RESTORED/ROOT/DATA.BIN"
    test "$(od -An -tu1 -N1 "$compare_source" | tr -d ' ')" = 77
    cp "$compare_source" "$OUT/BAD.BIN"
    dd if=/dev/zero of="$OUT/BAD.BIN" bs=1 count=1 conv=notrunc status=none
    head -c "$(( $(wc -c < "$compare_source") - 1 ))" "$compare_source" > "$OUT/SHORT.BIN"
    cp "$compare_source" "$OUT/LONG.BIN"
    printf X >> "$OUT/LONG.BIN"
    mcopy -i "$OUT/boot.img" "$OUT/BAD.BIN" ::/BADCRC/ROOT/DATA.BIN
    mcopy -i "$OUT/boot.img" "$OUT/SHORT.BIN" ::/SHORT/ROOT/DATA.BIN
    mcopy -i "$OUT/boot.img" "$OUT/LONG.BIN" ::/LONG/ROOT/DATA.BIN
    printf 'ROOT/SUB/F39.BIN\r\nROOT/SUB/ZERO.BIN\r\n' > "$OUT/TWO.LST"
    mcopy -i "$OUT/boot.img" "$OUT/TWO.LST" ::/
    mcopy -i "$OUT/boot.img" "$ROOT/tests/fixtures/EMPTY.BIN" ::/EMPTY.LST
fi
if test "$tape_test" = tree-filter || test "$tape_test" = tree-preview; then
    mmd -i "$OUT/boot.img" ::/PICKED ::/PICKED/ROOT ::/PICKED/ROOT/SUB
    for filter_target in F39.BIN ZERO.BIN DATA.BIN; do
        mcopy -i "$OUT/boot.img" "$ROOT/LICENSE" "::/PICKED/ROOT/SUB/$filter_target"
    done
    mattrib -i "$OUT/boot.img" +r +h +s ::/PICKED/ROOT/SUB/DATA.BIN
    mcopy -i "$OUT/boot.img" "$ROOT/LICENSE" ::/PICKED/KEEP.TXT
    printf 'ROOT/SUB/F39.BIN\r\n' > "$OUT/ONLY.LST"
    printf 'ROOT/*.BIN\r\n' > "$OUT/BADONLY.LST"
    mcopy -i "$OUT/boot.img" "$OUT/ONLY.LST" "$OUT/BADONLY.LST" ::/
    mcopy -i "$OUT/boot.img" "$ROOT/tests/fixtures/EMPTY.BIN" ::/EMPTY.LST
    mattrib -i "$OUT/boot.img" ::/PICKED/ROOT/SUB/DATA.BIN > "$OUT/filter-before.log"
fi
if test "$tape_test" = tree-preview; then
    mattrib -i "$OUT/boot.img" +r +h +s ::/PICKED/ROOT/SUB/F39.BIN
    mmd -i "$OUT/boot.img" ::/RECOVER ::/BLOCKED ::/BLOCKED/ROOT
    mcopy -i "$OUT/boot.img" "$ROOT/LICENSE" '::/RECOVER/OT$PART.TMP'
    mcopy -i "$OUT/boot.img" "$ROOT/LICENSE" ::/BLOCKED/ROOT/SUB
fi
if test "$tape_test" = tree-catalog; then
    mcopy -i "$OUT/boot.img" "$ROOT/LICENSE" '::/SRC/EMPTY/OT$LIST.TMP'
fi
if test "$tape_test" = tree-replace || test "$tape_test" = tree-list; then
    mmd -i "$OUT/boot.img" ::/RESTORED ::/RESTORED/ROOT ::/RESTORED/ROOT/SUB
    for replace_target in DATA.BIN SUB/DATA.BIN SUB/ZERO.BIN; do
        mcopy -m -i "$OUT/boot.img" "$ROOT/LICENSE" "::/RESTORED/ROOT/$replace_target"
    done
    mcopy -m -i "$OUT/boot.img" "$ROOT/LICENSE" ::/RESTORED/KEEP.TXT
    mattrib -i "$OUT/boot.img" +r +h +s ::/RESTORED/ROOT/SUB/DATA.BIN ::/RESTORED/ROOT/SUB/ZERO.BIN
    mattrib -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/ZERO.BIN > "$OUT/unselected-before.log"
    if test "$tape_test" = tree-list; then
        test "$tree_large" = 1 || { echo 'List fixture requires large tree' >&2; exit 1; }
        {
            printf '# Explicit replacement paths\r\nROOT/SUB/DATA.BIN\r\n'
            for list_index in $(seq -w 0 39); do
                printf 'ROOT/SUB/F%s.BIN\r\n' "$list_index"
            done
        } > "$OUT/SELECT.LST"
        for list_index in $(seq -w 0 39); do
            mcopy -m -i "$OUT/boot.img" "$OUT/QEXIT.COM" "::/RESTORED/ROOT/SUB/F$list_index.BIN"
        done
        printf 'ROOT/DATA.BIN\r\nROOT/*.BIN\r\n' > "$OUT/BAD.LST"
        printf 'root/data.bin\r\n' > "$OUT/DUP.LST"
        mcopy -i "$OUT/boot.img" "$OUT/SELECT.LST" "$OUT/BAD.LST" "$OUT/DUP.LST" ::/
    fi
fi
# A regular file with a device-like name must never be accepted as a tape.
mcopy -o -i "$OUT/boot.img" "$ROOT/LICENSE" ::/NRMT3
address=$(basename "$(readlink -f "/sys/class/scsi_generic/$(basename "$MHVTL_TAPE")/device")")
IFS=: read -r host channel target lun <<< "$address"
guest_target=$((10#$target % 8))
verify_tape
if [[ "$tape_test" = write || "$tape_test" = *-write ]]; then verify_writable; fi
if test "$read_test" = 1 || test "$native_test" = 1 || test "$tape_test" != none; then
    # Explicitly gated, host-side rewind of the approved virtual fixture only.
    # Rewind/export/filter/preview modes may explicitly reposition, never write tape data.
    sg_raw "$MHVTL_TAPE" 01 00 00 00 00 00 > "$OUT/rewind.log" 2>&1
fi
: > "$OUT/serial.log"
export_drive=()
if test "$ui_export" = 1; then
    mformat -C -i "$OUT/export.img" -f 1440 ::
    mcopy -i "$OUT/export.img" "$ROOT/LICENSE" ::/KEEP.TXT
    export_drive=(-drive "if=floppy,index=1,format=raw,file=$OUT/export.img")
fi
if test "$tape_test" = tree-preview || test "$tape_test" = tree-compare || test "$tape_test" = tree-ui; then
    cp "$OUT/boot.img" "$OUT/before.img"
fi
mkfifo "$OUT/monitor.fifo"
{
    sleep 3
    printf '%s\n' \
        "drive_add 0 if=none,id=tape,format=raw,file=$MHVTL_TAPE" \
        "device_add scsi-generic,drive=tape,bus=scsi0.0,scsi-id=$guest_target,lun=$lun"
    if test "$tape_test" = tree-ui; then source "$ROOT/tests/dos_ui_monitor.sh"; fi
} > "$OUT/monitor.fifo" &
writer=$!
trap 'kill "$writer" 2>/dev/null || true; rm -f -- "$OUT/monitor.fifo"' EXIT
set +e
test_timeout=${TIMEOUT:-45}
if test "$tape_test" = tree-ui; then test_timeout=${TIMEOUT:-90}; fi
timeout "$test_timeout" "$QEMU_BIN" -machine pc -m 16 -display none -vga std \
    -monitor stdio -drive if=floppy,format=raw,file="$OUT/boot.img" -boot a \
    "${export_drive[@]}" \
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
grep -q 'OpenTape archive state verification tests passed' "$OUT/serial.log"
grep -q 'OpenTape archive writer tests passed' "$OUT/serial.log"
grep -q 'Tape output tests passed' "$OUT/serial.log"
grep -q 'OpenTape source-file tests passed' "$OUT/serial.log"
grep -q 'OpenTape source readback verification tests passed' "$OUT/serial.log"
grep -q 'OpenTape backup-plan tests passed' "$OUT/serial.log"
grep -q 'OpenTape streaming verification tests passed' "$OUT/serial.log"
grep -q 'OpenTape restore tests passed' "$OUT/serial.log"
grep -q 'OpenTape selective restore tests passed' "$OUT/serial.log"
grep -q 'OpenTape replacement-list tests passed' "$OUT/serial.log"
grep -q 'OpenTape explicit rewind tests passed' "$OUT/serial.log"
grep -q 'OpenTape verification session lifecycle tests passed: 22 cases' "$OUT/serial.log"
grep -q 'OpenTape bounded archive catalogue tests passed' "$OUT/serial.log"
grep -q 'OpenTape catalogue marking tests passed' "$OUT/serial.log"
grep -q 'OpenTape catalogue selection conversion tests passed' "$OUT/serial.log"
grep -q 'OpenTape read-only tape probe tests passed' "$OUT/serial.log"
grep -q 'OpenTape device enumeration tests passed' "$OUT/serial.log"
grep -q "NRMT0 ASPI 00:0$guest_target:0$lun" "$OUT/serial.log"
if grep -q 'OpenTape DOS status FAILED' "$OUT/serial.log"; then
    echo 'DOS batch reported failure' >&2
    exit 1
fi
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
if test "$tape_test" = write; then
    grep -q 'OpenTape native tape write passed' "$OUT/serial.log"
elif test "$tape_test" = verify; then
    grep -q 'OpenTape native tape readback passed' "$OUT/serial.log"
    grep -q 'readback rc=0 DOS=0 blocks=11' "$OUT/serial.log"
elif test "$tape_test" = cli-verify; then
    grep -Eq '(Framing verified: 11 blocks, 11 records|Formatrahmen geprueft: 11 Bloecke, 11 Datensaetze)' "$OUT/serial.log"
    grep -Eq '(Session data verified: 1 closed sessions|Sitzungsdaten geprueft: 1 abgeschlossene Sitzungen)' "$OUT/serial.log"
elif test "$tape_test" = file-write; then
    grep -Eq '(File session written|Dateisitzung geschrieben): FILE.BIN,' "$OUT/serial.log"
elif test "$tape_test" = file-read; then
    grep -q 'OpenTape real-file readback passed:' "$OUT/serial.log"
elif test "$tape_test" = file-verify; then
    grep -Eq '(Session data verified: 1 closed sessions|Sitzungsdaten geprueft: 1 abgeschlossene Sitzungen)' "$OUT/serial.log"
elif test "$tape_test" = tree-write; then
    tree_files=3
    if test "$tree_large" = 1; then tree_files=43; fi
    grep -Eq "(Tree session written: $tree_files files, 3 directories|Baumsitzung geschrieben: $tree_files Dateien, 3 Verzeichnisse)" "$OUT/serial.log"
elif test "$tape_test" = tree-read; then
    tree_entries=6
    if test "$tree_large" = 1; then tree_entries=46; fi
    grep -q "OpenTape tree readback passed: $tree_entries entries," "$OUT/serial.log"
elif test "$tape_test" = tree-verify; then
    grep -Eq '(Session data verified: 1 closed sessions|Sitzungsdaten geprueft: 1 abgeschlossene Sitzungen)' "$OUT/serial.log"
    if test "$tree_large" = 1; then
        grep -Eq '(Framing verified: 95 blocks, 95 records|Formatrahmen geprueft: 95 Bloecke, 95 Datensaetze)' "$OUT/serial.log"
    fi
elif test "$tape_test" = tree-limited; then
    grep -Eq '(Read limit reached|Leselimit erreicht)' "$OUT/serial.log"
    if grep -Eq '(Session data verified|Sitzungsdaten geprueft):' "$OUT/serial.log"; then exit 1; fi
elif test "$tape_test" = tree-ui; then
    grep -Eq '(Status: not queried|Status: nicht abgefragt)' "$OUT/ui-start.txt"
    grep -Eq '(Status: not queried|Status: nicht abgefragt)' "$OUT/ui-select1.txt"
    grep -Fq 'NRMT0 FOUND ASPI' "$OUT/ui-devices.txt"
    grep -Fq 'NRMT3 REJECTED' "$OUT/ui-devices.txt"
    grep -Eq '(Selected: NRMT1|Ausgewaehlt: NRMT1)' "$OUT/ui-refresh1.txt"
    if grep -q 'state=' "$OUT/ui-rejected.txt"; then exit 1; fi
    grep -Eq '(Status: not queried|Status: nicht abgefragt)' "$OUT/ui-wrap0.txt"
    grep -Fq 'NRST2$ MISSING' "$OUT/ui-customscan.txt"
    grep -Eq '(No tape writes|Kein Bandschreib\.)' "$OUT/ui-status.txt"
    for label in cancel-enter cancel-escape cancel-select verify-cancel-enter verify-cancel-escape verify-cancel-select verify-cancel-rewind catalog-cancel-enter catalog-cancel-escape; do
        grep -Eq '(Selected: NRMT0|Ausgewaehlt: NRMT0)' "$OUT/ui-$label.txt"
        grep -Fq 'state=0002 block=0' "$OUT/ui-$label.txt"
    done
    for label in verify-rejected verify-custom verify-again catalog-rejected catalog-custom catalog-again; do
        grep -Eq '(INCOMPLETE;|UNVOLLSTAENDIG;)' "$OUT/ui-$label.txt"
        if grep -Eq '(Archive verification OK|Archivpruefung OK|FOUND ASPI|Scan [0-9]|state=)' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    for label in catalog-first catalog-last catalog-reload catalog-ignored; do
        grep -Eq '(Verified archive contents|Gepruefter Archivinhalt)' "$OUT/ui-$label.txt"
        grep -Eq '(Selected: NRMT0|Ausgewaehlt: NRMT0)' "$OUT/ui-$label.txt"
        grep -Eq '(First filemark; no restore authority|Erste Bandmarke; keine Restore-Freigabe)' "$OUT/ui-$label.txt"
    done
    grep -Fq 'ROOT/EMPTY' "$OUT/ui-catalog-first.txt"
    grep -Fq 'ROOT/SUB/F39.BIN' "$OUT/ui-catalog-last.txt"
    grep -Eq '\[x\] F +[0-9]+ ROOT/SUB/DATA.BIN' "$OUT/ui-mark-one.txt"
    grep -Eq '\[x\] F +[0-9]+ ROOT/DATA.BIN' "$OUT/ui-mark-two.txt"
    for label in mark-directory mark-none catalog-reload; do
        grep -Eq '(Marked: 0 files|Auswahl: 0 Dateien)' "$OUT/ui-$label.txt"
        if grep -Fq '[x]' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    for label in mark-preserved mark-all; do
        grep -Fq '[-] D' "$OUT/ui-$label.txt"
        grep -Eq '\[x\] F +[0-9]+ ROOT/SUB/DATA.BIN' "$OUT/ui-$label.txt"
        if grep -Eq '\[x\] D' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    for label in catalog-next catalog-last catalog-ignored; do
        grep -Eq '(Marked: 1 files|Auswahl: 1 Dateien)' "$OUT/ui-$label.txt"
    done
    if test "$ui_export" = 1; then
        grep -Eq '(Marked: 2 files|Auswahl: 2 Dateien)' "$OUT/ui-export-existing.txt"
        grep -Fq 'B:\PICKED.LST' "$OUT/ui-export-prompt.txt"
        mtype -i "$OUT/export.img" ::/PICKED.LST > "$OUT/exported.lst"
        printf '%s\r\n' ROOT/SUB/DATA.BIN ROOT/DATA.BIN > "$OUT/expected.lst"
        cmp "$OUT/expected.lst" "$OUT/exported.lst"
        mtype -i "$OUT/export.img" ::/KEEP.TXT | cmp - "$ROOT/LICENSE"
        mdir -b -i "$OUT/export.img" ::/ > "$OUT/export-files.txt"
        test "$(wc -l < "$OUT/export-files.txt")" -eq 2
        if mdir -i "$OUT/export.img" '::/OT$LIST.TMP' >/dev/null 2>&1; then exit 1; fi
    fi
    test "$(od -An -tu1 -j1447 -N1 "$OUT/ui-catalog-first.bin" | tr -d ' ')" = 30
    test "$(od -An -tu1 -j1607 -N1 "$OUT/ui-catalog-first.bin" | tr -d ' ')" = 31
    for label in catalog-back catalog-escape; do
        grep -Eq '(Status: not queried|Status: nicht abgefragt)' "$OUT/ui-$label.txt"
        if grep -Eq '(ROOT/|Verified archive contents|Gepruefter Archivinhalt)' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    for label in verify-done verify-ignored; do
        grep -Eq '(Accepted: 95 blocks, 95 records|Akzeptiert: 95 Bloecke, 95 Datensaetze)' "$OUT/ui-$label.txt"
        grep -Eq '(Closed sessions: 1|Abgeschlossene Sitzungen: 1)' "$OUT/ui-$label.txt"
        grep -Eq '(To first filemark; not whole-volume test|Bis erste Bandmarke; kein Gesamtbandtest)' "$OUT/ui-$label.txt"
        if grep -Eq '(FOUND ASPI|Scan [0-9]|state=)' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    for label in verify-prompt verify-cancel-enter-prompt; do
        grep -Eq '(To first filemark; not whole-volume test|Bis erste Bandmarke; kein Gesamtbandtest)' "$OUT/ui-$label.txt"
    done
    for label in rejected-rewind missing-rewind rewind-done; do
        grep -Eq '(Status: not queried|Status: nicht abgefragt)' "$OUT/ui-$label.txt"
        if grep -Eq '(FOUND ASPI|Scan [0-9]|state=)' "$OUT/ui-$label.txt"; then exit 1; fi
    done
    test "$(od -An -tu1 -j1767 -N1 "$OUT/ui-select1.bin" | tr -d ' ')" = 30
    test "$(od -An -tu1 -j1607 -N1 "$OUT/ui-select1.bin" | tr -d ' ')" = 31
    test "$(od -An -tu1 -j917 -N1 "$OUT/ui-select1.bin" | tr -d ' ')" = 127
    grep -Eq '(Framing verified: 95 blocks, 95 records|Formatrahmen geprueft: 95 Bloecke, 95 Datensaetze)' "$OUT/serial.log"
    test "$(grep -Ec '(Framing verified: 95 blocks, 95 records|Formatrahmen geprueft: 95 Bloecke, 95 Datensaetze)' "$OUT/serial.log")" -eq 2
    cmp "$OUT/before.img" "$OUT/boot.img"
elif test "$tape_test" = tree-devices; then
    test "$(grep -c '^FOUND NRMT0 ASPI ' "$OUT/serial.log")" -eq 2
    test "$(grep -c '^REJECTED NRMT3 ' "$OUT/serial.log")" -eq 1
    test "$(grep -c '^MISSING ' "$OUT/serial.log")" -eq 13
    grep -Fq 'MISSING NRST0$ ' "$OUT/serial.log"
    grep -Fq 'MISSING NRRT0$ ' "$OUT/serial.log"
    grep -Eq '(Aliases checked: 4; found: 1; missing: 2; rejected: 1; errors: 0|Aliase geprueft: 4; gefunden: 1; fehlen: 2; abgelehnt: 1; Fehler: 0)' "$OUT/serial.log"
    grep -Eq '(Aliases checked: 4; found: 1; missing: 3; rejected: 0; errors: 0|Aliase geprueft: 4; gefunden: 1; fehlen: 3; abgelehnt: 0; Fehler: 0)' "$OUT/serial.log"
    test "$(grep -Ec '(Alias query complete;|Aliasabfrage vollstaendig;)' "$OUT/serial.log")" -eq 3
    grep -Eq '(Framing verified: 95 blocks, 95 records|Formatrahmen geprueft: 95 Bloecke, 95 Datensaetze)' "$OUT/serial.log"
elif test "$tape_test" = tree-compare; then
    test "$(grep -c '^MATCH ' "$OUT/serial.log")" -eq 45
    grep -Eq '(Comparison verified: 43 files, 3 archive directories|Vergleich geprueft: 43 Dateien, 3 Archivverzeichnisse)' "$OUT/serial.log"
    grep -Eq '(Comparison verified: 2 files, 0 archive directories|Vergleich geprueft: 2 Dateien, 0 Archivverzeichnisse)' "$OUT/serial.log"
    test "$(grep -Ec '(Comparison verified:|Vergleich geprueft:)' "$OUT/serial.log")" -eq 2
    test "$(grep -Ec '(File verification failed \(-103\)|Dateipruefung fehlgeschlagen \(-103\))' "$OUT/serial.log")" -eq 3
    grep -Eq '(Files excluded by filter: 41|Nicht ausgewaehlte Dateien: 41)' "$OUT/serial.log"
    if grep -Eq '(Session restored:|Sitzung wiederhergestellt:|Preview verified:|Vorschau geprueft:)' "$OUT/serial.log"; then exit 1; fi
    cmp "$OUT/before.img" "$OUT/boot.img"
elif test "$tape_test" = tree-readback; then
    grep -q 'OpenTape staged restore readback tests passed' "$OUT/serial.log"
    grep -Eq '(Session restored: 43 files, 3 directories|Sitzung wiederhergestellt: 43 Dateien, 3 Verzeichnisse)' "$OUT/serial.log"
    grep -Eq '(Staged files read back and checked: 43|Temporaere Dateien zurueckgelesen und geprueft: 43)' "$OUT/serial.log"
    mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/ZERO.BIN | cmp - "$ROOT/tests/fixtures/EMPTY.BIN"
    mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/F39.BIN | cmp - "$ROOT/tests/fixtures/EMPTY.BIN"
    for readback_nonempty in ROOT/DATA.BIN ROOT/SUB/DATA.BIN; do
        test "$(mtype -i "$OUT/boot.img" "::/RESTORED/$readback_nonempty" | wc -c)" -gt 0
    done
    mdir -a -b -s -i "$OUT/boot.img" ::/RESTORED > "$OUT/readback-tree.log"
    test "$(grep -vc '/$' "$OUT/readback-tree.log")" -eq 43
    mdir -i "$OUT/boot.img" ::/RESTORED/ROOT/EMPTY > "$OUT/readback-empty.log"
    for readback_temporary in 'OT$PART.TMP' 'OT$OLD.TMP'; do
        if mdir -i "$OUT/boot.img" "::/RESTORED/$readback_temporary" > "$OUT/readback-staging.log" 2>&1; then exit 1; fi
    done
elif test "$tape_test" = tree-preview; then
    test "$(grep -c '^NEW ' "$OUT/serial.log")" -eq 43
    test "$(grep -c '^REPLACE ' "$OUT/serial.log")" -eq 1
    test "$(grep -c '^KEEP ' "$OUT/serial.log")" -eq 1
    grep -Eq '^REPLACE +0 ROOT/SUB/F39.BIN' "$OUT/serial.log"
    grep -Eq '^KEEP +0 ROOT/SUB/ZERO.BIN' "$OUT/serial.log"
    grep -Eq '(Preview verified: 43 files planned, 3 archive directories|Vorschau geprueft: 43 Dateien geplant, 3 Archivverzeichnisse)' "$OUT/serial.log"
    grep -Eq '(Preview verified: 1 files planned, 0 archive directories|Vorschau geprueft: 1 Dateien geplant, 0 Archivverzeichnisse)' "$OUT/serial.log"
    test "$(grep -Ec '(Preview verified:|Vorschau geprueft:)' "$OUT/serial.log")" -eq 2
    grep -Eq '(Files excluded by filter: 41|Nicht ausgewaehlte Dateien: 41)' "$OUT/serial.log"
    grep -Eq '(Planned replacements: 1; existing files preserved: 1|Geplante Ersetzungen: 1; vorhandene Dateien beibehalten: 1)' "$OUT/serial.log"
    if grep -Eq '(Session restored:|Sitzung wiederhergestellt:)' "$OUT/serial.log"; then exit 1; fi
    cmp "$OUT/before.img" "$OUT/boot.img"
elif test "$tape_test" = tree-filter; then
    grep -Eq '(Session restored: 2 files, 0 directories|Sitzung wiederhergestellt: 2 Dateien, 0 Verzeichnisse)' "$OUT/serial.log"
    grep -Eq '(Session restored: 1 files, 0 directories|Sitzung wiederhergestellt: 1 Dateien, 0 Verzeichnisse)' "$OUT/serial.log"
    grep -Eq '(Replaced files: 1; existing files unchanged: 1|Ersetzte Dateien: 1; vorhandene unveraendert: 1)' "$OUT/serial.log"
    test "$(grep -Ec '(Files excluded by filter: 41|Nicht ausgewaehlte Dateien: 41)' "$OUT/serial.log")" -eq 2
    for filtered_empty in NEWPICK/ROOT/SUB/F39.BIN NEWPICK/ROOT/SUB/ZERO.BIN PICKED/ROOT/SUB/F39.BIN; do
        mtype -i "$OUT/boot.img" "::/$filtered_empty" | cmp - "$ROOT/tests/fixtures/EMPTY.BIN"
    done
    for filter_preserved in PICKED/ROOT/SUB/ZERO.BIN PICKED/ROOT/SUB/DATA.BIN PICKED/KEEP.TXT; do
        mtype -i "$OUT/boot.img" "::/$filter_preserved" | cmp - "$ROOT/LICENSE"
    done
    mattrib -i "$OUT/boot.img" ::/PICKED/ROOT/SUB/DATA.BIN > "$OUT/filter-after.log"
    cmp "$OUT/filter-before.log" "$OUT/filter-after.log"
    for filter_root in NEWPICK PICKED NOHIT; do
        mdir -b -i "$OUT/boot.img" "::/$filter_root" > "$OUT/filter-$filter_root.log"
        if grep -Eiq 'OT\$(PART|OLD)\.TMP' "$OUT/filter-$filter_root.log"; then exit 1; fi
    done
elif test "$tape_test" = tree-catalog; then
    catalog_files=3; catalog_dirs=3; catalog_blocks=15
    if test "$tree_large" = 1; then catalog_files=43; catalog_blocks=95; fi
    test "$(grep -c '^F ' "$OUT/serial.log")" -eq "$catalog_files"
    test "$(grep -c '^D ' "$OUT/serial.log")" -eq "$catalog_dirs"
    grep -Fq ' ROOT/DATA.BIN' "$OUT/serial.log"
    grep -Fq ' ROOT/SUB/DATA.BIN' "$OUT/serial.log"
    grep -Fq ' ROOT/SUB/ZERO.BIN' "$OUT/serial.log"
    grep -Eq '(Contents listing verified through filemark|Inhaltsanzeige bis Bandmarke vollstaendig geprueft)' "$OUT/serial.log"
    grep -Eq "(Framing verified: $catalog_blocks blocks|Formatrahmen geprueft: $catalog_blocks Bloecke)" "$OUT/serial.log"
    mtype -i "$OUT/boot.img" ::/FILES.LST > "$OUT/exported.lst"
    test "$(wc -l < "$OUT/exported.lst")" -eq "$catalog_files"
    tr -d '\r' < "$OUT/exported.lst" | LC_ALL=C sort > "$OUT/exported-sorted.lst"
    {
        printf '%s\n' ROOT/DATA.BIN ROOT/SUB/DATA.BIN ROOT/SUB/ZERO.BIN
        if test "$tree_large" = 1; then
            for list_index in $(seq -w 0 39); do printf 'ROOT/SUB/F%s.BIN\n' "$list_index"; done
        fi
    } | LC_ALL=C sort > "$OUT/expected-sorted.lst"
    cmp "$OUT/expected-sorted.lst" "$OUT/exported-sorted.lst"
    mtype -i "$OUT/boot.img" '::/SRC/EMPTY/OT$LIST.TMP' | cmp - "$ROOT/LICENSE"
    test "$(grep -Ec '(rewound synchronously; status and close succeeded|synchron zurueckgespult; Status und Schliessen erfolgreich)' "$OUT/serial.log")" -eq 2
    test "$(grep -Ec "(Framing verified: $catalog_blocks blocks|Formatrahmen geprueft: $catalog_blocks Bloecke)" "$OUT/serial.log")" -eq 3
elif [[ "$tape_test" = tree-restore || "$tape_test" = tree-replace || "$tape_test" = tree-list ]]; then
    tree_files=3; tree_entries=6
    if test "$tree_large" = 1; then tree_files=43; tree_entries=46; fi
    if test "$tape_test" != tree-restore; then tree_files=$((tree_files - 1)); fi
    grep -Eq "(Session restored: $tree_files files, 3 directories|Sitzung wiederhergestellt: $tree_files Dateien, 3 Verzeichnisse)" "$OUT/serial.log"
    grep -q "OpenTape extracted tree comparison passed: $tree_entries entries" "$OUT/serial.log"
    mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/DATA.BIN | cmp - "$ROOT/build/$MAXLANG/dos/TAPETEST.EXE"
    mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/DATA.BIN | cmp - "$ROOT/build/$MAXLANG/dos/OUTTEST.EXE"
    if test "$tape_test" != tree-restore; then
        list_replaced=2
        if test "$tape_test" = tree-list; then list_replaced=42; fi
        grep -Eq "(Replaced files: $list_replaced; existing files unchanged: 1|Ersetzte Dateien: $list_replaced; vorhandene unveraendert: 1)" "$OUT/serial.log"
        mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/ZERO.BIN | cmp - "$ROOT/LICENSE"
        mtype -i "$OUT/boot.img" ::/RESTORED/KEEP.TXT | cmp - "$ROOT/LICENSE"
        mattrib -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/ZERO.BIN > "$OUT/unselected-after.log"
        cmp "$OUT/unselected-before.log" "$OUT/unselected-after.log"
    else
        mtype -i "$OUT/boot.img" ::/RESTORED/ROOT/SUB/ZERO.BIN | cmp - "$ROOT/tests/fixtures/EMPTY.BIN"
    fi
    mdir -i "$OUT/boot.img" ::/RESTORED/ROOT/EMPTY > "$OUT/empty-directory.log"
    if mdir -i "$OUT/boot.img" '::/RESTORED/OT$PART.TMP' > "$OUT/staging-check.log" 2>&1; then
        echo 'Unexpected staging file after successful restore' >&2; exit 1
    fi
    if mdir -i "$OUT/boot.img" '::/RESTORED/OT$OLD.TMP' > "$OUT/previous-check.log" 2>&1; then
        echo 'Unexpected previous-version file after successful restore' >&2; exit 1
    fi
fi
mtype -i "$OUT/boot.img" ::/NRMT3 | cmp - "$ROOT/LICENSE"
mtype -i "$OUT/boot.img" ::/SOURCE.BIN | cmp - "$ROOT/build/$MAXLANG/dos/TAPETEST.EXE"
echo "OpenTape $MAXLANG DOS smoke passed (read: $read_test, native rejection: $native_test, tape: $tape_test)"

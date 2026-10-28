if [[ "$variant" = set-* ]]; then
    cmp "$run_dir/before.img" "$run_dir/boot.img"
    if [[ "$variant" = set-cli* ]]; then
        grep -Fq 'DATA/SINGLE.BIN' "$run_dir/serial.log"
        grep -Fq 'DATA/TREE/NOTE.TXT' "$run_dir/serial.log"
    fi
elif test "$variant" = ui-set-depth-limit; then
    cmp "$run_dir/before.img" "$run_dir/boot.img"
elif [[ "$variant" = ui-set* ]]; then
    mcopy -i "$run_dir/boot.img" ::/OPENTAPE.CFG "$run_dir/set.cfg"
    python3 - "$run_dir/set.cfg" <<'PY'
import pathlib, struct, sys, zlib
p = pathlib.Path(sys.argv[1]).read_bytes()
assert p[:8] == b'OTCF\x02\x00\x51\x01' and len(p) == 337
assert zlib.crc32(p[:12]) == struct.unpack_from('<I', p, 12)[0]
assert zlib.crc32(p[16:]) == struct.unpack_from('<I', p, 8)[0]
assert p[16:18] == b'\x01\x02'
assert p[18:59].split(b'\0')[0] == b'SET.LST'
assert p[59:100].split(b'\0')[0] == b'DATA'
assert p[100:132].split(b'\0')[0] == b'TEST'
PY
    if mdir -i "$run_dir/boot.img" '::/OT$LIST.TMP' >/dev/null 2>&1; then exit 1; fi
fi

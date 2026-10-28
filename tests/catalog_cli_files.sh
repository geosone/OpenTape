# Prepare only private floppy images. No physical tape devices are involved.
if [[ "$variant" = catalog-cli-* ]] || test "$variant" = ui-catalog-start; then
    mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/CATIO.EXE" ::/
    python3 "$ROOT/tests/make_catalog_fixture.py" "$run_dir/OFFLINE.CAT"
    if test "$variant" = catalog-cli-empty; then
        python3 - "$run_dir/OFFLINE.CAT" <<'PY'
import pathlib, struct, sys, zlib
p = pathlib.Path(sys.argv[1]); data = bytearray(p.read_bytes()[:120])
struct.pack_into('<H', data, 12, 0); struct.pack_into('<II', data, 16, 88, zlib.crc32(data[32:]))
struct.pack_into('<I', data, 28, zlib.crc32(data[:28])); p.write_bytes(data)
PY
    fi
    mcopy -i "$run_dir/boot.img" "$run_dir/OFFLINE.CAT" ::/OPENTAPE.CAT
    if test "$variant" = catalog-cli-invalid; then
        printf 'broken catalogue' > "$run_dir/BROKEN.CAT"
        mcopy -i "$run_dir/boot.img" "$run_dir/BROKEN.CAT" ::/
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RMT0.CAT
    fi
    if test "$variant" = catalog-cli-staging; then
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" '::/OT$LIST.TMP'
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/BATCH.CAT
    fi
    if test "$variant" = catalog-cli-large; then mdel -i "$run_dir/boot.img" ::/OPENTAPE.CAT; fi
    cp "$run_dir/boot.img" "$run_dir/before.img"
fi

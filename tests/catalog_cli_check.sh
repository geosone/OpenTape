# Exact fixture counts are checked separately by COUNTS.COM.
if [[ "$variant" = catalog-cli-* ]]; then
    case "$variant" in
        catalog-cli-live|catalog-cli-legacy|catalog-cli-fixed)
            mcopy -i "$run_dir/boot.img" ::/BATCH.CAT "$run_dir/batch.cat"
            python3 "$ROOT/tests/check_catalog_file.py" "$run_dir/batch.cat"
            grep -Eq '(OFFLINE catalogue: metadata only|OFFLINE-Katalog: nur Metadaten)' "$run_dir/serial.log"
            grep -Eq '(Catalogue entries: 1|Katalogeintraege: 1)' "$run_dir/serial.log"
            if mdir -i "$run_dir/boot.img" '::/OT$LIST.TMP' >/dev/null 2>&1; then exit 1; fi ;;
        catalog-cli-corrupt)
            cmp "$run_dir/before.img" "$run_dir/boot.img"
            if mdir -i "$run_dir/boot.img" ::/BATCH.CAT >/dev/null 2>&1; then exit 1; fi
            if grep -Fq 'DATA/INPUT.BIN' "$run_dir/serial.log"; then
                # The source name is printed by backup; no metadata row is allowed.
                if grep -Eq '^F +35000 ' "$run_dir/serial.log"; then exit 1; fi
            fi ;;
        catalog-cli-large)
            mcopy -i "$run_dir/boot.img" ::/OPENTAPE.CAT "$run_dir/large.cat"
            python3 "$ROOT/tests/check_catalog_file.py" "$run_dir/large.cat" large
            grep -Eq '(Catalogue entries: 256|Katalogeintraege: 256)' "$run_dir/serial.log"
            test "$(grep -Ec '^F +[0-9]+ ' "$run_dir/serial.log")" = 256 ;;
        catalog-cli-offline|catalog-cli-empty)
            cmp "$run_dir/before.img" "$run_dir/boot.img"
            grep -Eq '(OFFLINE catalogue: metadata only|OFFLINE-Katalog: nur Metadaten)' "$run_dir/serial.log"
            count=1; if test "$variant" = catalog-cli-empty; then count=0; fi
            grep -Eq "(Catalogue entries: $count|Katalogeintraege: $count)" "$run_dir/serial.log" ;;
        *) cmp "$run_dir/before.img" "$run_dir/boot.img" ;;
    esac
elif test "$variant" = ui-catalog-start; then cmp "$run_dir/before.img" "$run_dir/boot.img"; fi

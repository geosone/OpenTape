if [[ "$variant" = set-* ]] || [[ "$variant" = ui-set* ]]; then
    mcopy -i "$run_dir/boot.img" "$OUT/bin-$MAXLANG/BSETTEST.EXE" ::/
    printf '# Independent file and directory roots\r\nF INPUT.BIN SINGLE.BIN\r\nT SRC TREE\r\n' > "$run_dir/SET.LST"
    printf 'second root payload\n' > "$run_dir/NOTE.TXT"
    mmd -i "$run_dir/boot.img" ::/SRC ::/SRC/EMPTY ::/CHECK ::/CHECK/DATA ::/CHECK/DATA/TREE ::/CHECK/DATA/TREE/EMPTY
    mcopy -i "$run_dir/boot.img" "$run_dir/SET.LST" ::/
    mcopy -i "$run_dir/boot.img" "$run_dir/NOTE.TXT" ::/SRC/
    mcopy -i "$run_dir/boot.img" "$run_dir/NOTE.TXT" ::/CHECK/DATA/TREE/
    mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" ::/CHECK/DATA/SINGLE.BIN
    if [[ "$variant" = ui-set-depth* ]]; then
        mdel -i "$run_dir/boot.img" ::/SRC/NOTE.TXT
        mrd -i "$run_dir/boot.img" ::/SRC/EMPTY
        printf 'T SRC TREE\n' > "$run_dir/SET.LST"
        mcopy -o -i "$run_dir/boot.img" "$run_dir/SET.LST" ::/
        depth=16; if test "$variant" = ui-set-depth-limit; then depth=17; fi
        disk_path='::/SRC'; mirror_path='::/CHECK/DATA/TREE'
        for n in $(seq 1 "$depth"); do
            printf -v part 'D%02d' "$n"
            disk_path="$disk_path/$part"; mirror_path="$mirror_path/$part"
            mmd -i "$run_dir/boot.img" "$disk_path" "$mirror_path"
        done
        mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$disk_path/INPUT.BIN"
        mcopy -i "$run_dir/boot.img" "$run_dir/INPUT.BIN" "$mirror_path/INPUT.BIN"
    fi
    if test "$variant" = set-invalid; then
        printf 'F INPUT.BIN FIRST.BIN\nunknown\n' > "$run_dir/BAD.LST"
        printf '# nothing to back up\n' > "$run_dir/EMPTY.LST"
        printf 'F INPUT.BIN SAME.BIN\nF SRC/NOTE.TXT same.bin\n' > "$run_dir/DUP.LST"
        printf 'F INPUT.BIN TARGET\nT SRC target/SUB\n' > "$run_dir/PREFIX.LST"
        printf 'F RMT0.CFG BAD.BIN\n' > "$run_dir/ALIAS.LST"
        printf 'T RMT0/. TREE\n' > "$run_dir/CANON.LST"
        for name in BAD EMPTY DUP PREFIX ALIAS CANON; do mcopy -i "$run_dir/boot.img" "$run_dir/$name.LST" ::/; done
        mcopy -i "$run_dir/boot.img" "$ROOT/LICENSE" ::/RMT0.CFG
    fi
    if test "$variant" = set-plan-limit; then
        mmd -i "$run_dir/boot.img" ::/LIMIT
        printf 'F INPUT.BIN FIRST.BIN\nT LIMIT MANY\n' > "$run_dir/LIMIT.LST"
        mcopy -i "$run_dir/boot.img" "$run_dir/LIMIT.LST" ::/
        : > "$run_dir/EMPTY.BIN"
        for n in $(seq 0 254); do printf -v name 'F%03d.BIN' "$n"; mcopy -i "$run_dir/boot.img" "$run_dir/EMPTY.BIN" "::/LIMIT/$name"; done
    fi
    cp "$run_dir/boot.img" "$run_dir/before.img"
fi

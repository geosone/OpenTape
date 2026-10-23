# Sourced by the smoke harness's background monitor writer; stdout is HMP.
# Capture real VGA text/attributes, not a production-only UI test hook.
ui_capture() {
    local label=$1 pattern=$2 attempt
    # Never accept a matching VGA dump left by a previous test run.
    rm -f -- "$OUT/ui-$label.bin" "$OUT/ui-$label.txt"
    for attempt in $(seq 1 120); do
        printf 'pmemsave 0xb8000 0xfa0 "%s/ui-%s.bin"\n' "$OUT" "$label"
        sleep 0.2
        if test -s "$OUT/ui-$label.bin"; then
            od -An -v -tu1 "$OUT/ui-$label.bin" | awk '
                { for (i=1; i<=NF; i++) if ((bytes++ % 2)==0) {
                    c=$i; printf "%c", c>=32 && c<=126 ? c : 32;
                    if ((++chars % 80)==0) printf "\n";
                } }' > "$OUT/ui-$label.txt"
            # This help row is rendered last. A matching alias near the top
            # alone can be visible while the lower status panel is unfinished.
            if grep -Eq "$pattern" "$OUT/ui-$label.txt" &&
               grep -Eq '(FOUND != ready|FOUND != bereit)' "$OUT/ui-$label.txt"; then return 0; fi
        fi
    done
    echo "UI checkpoint failed: $label ($pattern)" >&2
    printf 'quit\n'
    exit 1
}
ui_key() { printf 'sendkey %s\n' "$1"; }

# Real QEMU PS/2 packets through CuteMouse /R00 and QUILIB INT 33h polling.
# Keep deltas in signed-byte range: large HMP jumps can set PS/2 overflow bits
# and are not a reliable way to position a DOS driver's software cursor.
ui_mouse_relative() {
    local dx=$1 dy=$2 step_x step_y
    while test "$dx" != 0 || test "$dy" != 0; do
        step_x=$dx; step_y=$dy
        if ((step_x>100)); then step_x=100; elif ((step_x< -100)); then step_x=-100; fi
        if ((step_y>100)); then step_y=100; elif ((step_y< -100)); then step_y=-100; fi
        printf 'mouse_move %d %d\n' "$step_x" "$step_y"
        dx=$((dx-step_x)); dy=$((dy-step_y)); sleep 0.08
    done
}
ui_mouse_point() {
    ui_mouse_relative -1000 -1000
    # /R00 stores sensitivity 91/256; larger packets multiply it by 3.
    # Convert text pixels to mickeys with that 273/256 coefficient and the
    # driver's default horizontal/vertical ratios of 8/16 per 8 pixels.
    ui_mouse_relative "$(( ($1 * 8 + 4) * 256 / 273 ))" "$(( ($2 * 8 + 4) * 2 * 256 / 273 ))"
    sleep 0.12
}
ui_mouse_click() {
    ui_mouse_point "$1" "$2"
    printf 'mouse_button 1\n'; sleep 0.12
    printf 'mouse_button 0\n'; sleep 0.15
}

source "$ROOT/tests/ui_monitor_helpers.sh"
source "$ROOT/tests/mouse_monitor_helpers.sh"
ui_capture mouse-device '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 14
ui_capture mouse-backup-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
# Repeating the opener coordinates does not touch a confirmation button.
ui_mouse_click 60 14
ui_capture mouse-opener-not-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
# Press confirmation, drag out and release: no operation.
ui_mouse_point 5 21
printf 'mouse_button 1\n'; sleep 0.12
ui_mouse_relative 240 0; sleep 0.2
printf 'mouse_button 0\n'; sleep 0.15
ui_capture mouse-drag-cancelled '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
ui_mouse_click 25 21
ui_capture mouse-cancelled '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 14
ui_capture mouse-fresh-confirm '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
ui_mouse_click 5 21
ui_capture mouse-backup-written '(Session written; tape closed.|Sitzung geschrieben; Band geschlossen.)'
ui_mouse_click 60 5
ui_capture mouse-result-back '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 14
ui_capture mouse-new-grant '(OVERWRITES at CURRENT tape position.|UEBERSCHREIBT ab AKTUELLER Bandposition.)'
ui_mouse_click 25 21
ui_capture mouse-new-cancelled '(Devices and tape status|Geraete und Bandstatus)'
ui_mouse_click 60 13

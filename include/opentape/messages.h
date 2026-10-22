#ifndef OPENTAPE_MESSAGES_H
#define OPENTAPE_MESSAGES_H

/* Open386MAX-compatible build languages: US (default) and GR (German). */
#ifdef LANG_GR

#define OT_MSG_APP_TITLE          "OpenTape"
#define OT_MSG_VERSION            "OpenTape 0.1 Startversion"
#define OT_MSG_USAGE              "Aufruf: OPENTAPE [--help | --status [--device NRMT0]]"
#define OT_MSG_READ_USAGE         "Lesetest: OPENTAPE --read-test [--device NRMT0]"
#define OT_MSG_READ_WARNING       "Lesen ab aktueller Position, maximal 16 Bloecke; kein Zurueckspulen."
#define OT_MSG_READ_BLOCK         "Block %u: %u Byte CRC32=%08lX\n"
#define OT_MSG_READ_EOF           "Dateimarke: %u Bloecke, %lu Byte\n"
#define OT_MSG_READ_LIMIT         "Leselimit erreicht; Dateiende noch nicht bestaetigt."
#define OT_MSG_READ_MEMORY        "Nicht genug Speicher fuer den Lesepuffer."
#define OT_MSG_UNKNOWN_OPTION     "OpenTape: unbekannte Option"
#define OT_MSG_TASK_SUMMARY       "Sichern, wiederherstellen, pruefen, katalogisieren"
#define OT_MSG_TASK_SUMMARY_LONG  "Sicherung und Wiederherstellung fuer SCSI-Band"
#define OT_MSG_NOT_IMPLEMENTED    "Sichern/Wiederherstellen ist noch nicht implementiert."
#define OT_MSG_PROBE_ERROR        "Bandstatus nicht verfuegbar: %s (Fehler %d, DOS %u)\n"
#define OT_MSG_PROBE_STATUS       "%s ASPI %02X:%02X:%02X Status=%04X Block=%u\n"
#define OT_MSG_PROBE_SCSI         "SCSI=%02X Sense=%02X Befehl=%04X Rest=%u\n"
#define OT_MSG_PROBE_ABI          "Status-ABI: %u Byte; Treiberfehler: %u\n"
#define OT_MSG_PROBE_LEGACY       "Status-ABI: 14 Byte; Treiberfehler: nicht verfuegbar"
#define OT_MSG_PROBE_READONLY     "Nur Status; keine Bandbewegung oder Schreibzugriffe."
#define OT_MSG_BACKEND_FMT        "Erkanntes Backend: %s"
#define OT_MSG_HELP_TITLE         "OpenTape-Hilfe"
#define OT_MSG_ESCAPE_DOS         "Esc kehrt zu DOS zurueck."

#define OT_MSG_TASK_BACKUP        "Sichern..."
#define OT_MSG_TASK_RESTORE       "Wiederherstellen..."
#define OT_MSG_TASK_VERIFY        "Pruefen..."
#define OT_MSG_TASK_CATALOGUE     "Katalog..."
#define OT_MSG_TASK_MEDIA         "Medium..."
#define OT_MSG_TASK_SETTINGS      "Einstellungen..."

#define OT_MSG_MENU_FILE          "&Datei"
#define OT_MSG_MENU_EXIT          "&Beenden"
#define OT_MSG_MENU_HELP          "&Hilfe"
#define OT_MSG_MENU_ABOUT         "&Info zu OpenTape..."
#define OT_MSG_ABOUT_TITLE        "Info zu OpenTape"
#define OT_MSG_ABOUT_TEXT         "OpenTape 0.1 Startversion\nGPL Version 3"

#define OT_MSG_BACKEND_AUTO       "automatisch"
#define OT_MSG_BACKEND_LEGACY     "ASPITAPE-kompatibel"
#define OT_MSG_BACKEND_OPENASPI   "OpenASPI"
#define OT_MSG_BACKEND_DIRECT     "direktes ASPI"
#define OT_MSG_BACKEND_NONE       "keines"

#else

#define OT_MSG_APP_TITLE          "OpenTape"
#define OT_MSG_VERSION            "OpenTape 0.1 bootstrap"
#define OT_MSG_USAGE              "Usage: OPENTAPE [--help | --status [--device NRMT0]]"
#define OT_MSG_READ_USAGE         "Read test: OPENTAPE --read-test [--device NRMT0]"
#define OT_MSG_READ_WARNING       "Reading at current position, at most 16 blocks; no rewind."
#define OT_MSG_READ_BLOCK         "Block %u: %u bytes CRC32=%08lX\n"
#define OT_MSG_READ_EOF           "Filemark: %u blocks, %lu bytes\n"
#define OT_MSG_READ_LIMIT         "Read limit reached; end of file not yet confirmed."
#define OT_MSG_READ_MEMORY        "Not enough memory for the read buffer."
#define OT_MSG_UNKNOWN_OPTION     "OpenTape: unknown option"
#define OT_MSG_TASK_SUMMARY       "Backup, restore, verify and catalogue"
#define OT_MSG_TASK_SUMMARY_LONG  "Backup, restore, verify and catalogue for SCSI tape"
#define OT_MSG_NOT_IMPLEMENTED    "Backup/restore is not implemented yet."
#define OT_MSG_PROBE_ERROR        "Tape status unavailable: %s (error %d, DOS %u)\n"
#define OT_MSG_PROBE_STATUS       "%s ASPI %02X:%02X:%02X state=%04X block=%u\n"
#define OT_MSG_PROBE_SCSI         "SCSI=%02X sense=%02X opcode=%04X residual=%u\n"
#define OT_MSG_PROBE_ABI          "Status ABI: %u bytes; driver error: %u\n"
#define OT_MSG_PROBE_LEGACY       "Status ABI: 14 bytes; driver error: unavailable"
#define OT_MSG_PROBE_READONLY     "Status only; no tape movement or writes."
#define OT_MSG_BACKEND_FMT        "Detected backend: %s"
#define OT_MSG_HELP_TITLE         "OpenTape help"
#define OT_MSG_ESCAPE_DOS         "Esc returns to DOS."

#define OT_MSG_TASK_BACKUP        "Backup..."
#define OT_MSG_TASK_RESTORE       "Restore..."
#define OT_MSG_TASK_VERIFY        "Verify..."
#define OT_MSG_TASK_CATALOGUE     "Catalogue..."
#define OT_MSG_TASK_MEDIA         "Media..."
#define OT_MSG_TASK_SETTINGS      "Settings..."

#define OT_MSG_MENU_FILE          "&File"
#define OT_MSG_MENU_EXIT          "E&xit"
#define OT_MSG_MENU_HELP          "&Help"
#define OT_MSG_MENU_ABOUT         "&About OpenTape..."
#define OT_MSG_ABOUT_TITLE        "About OpenTape"
#define OT_MSG_ABOUT_TEXT         "OpenTape 0.1 bootstrap\nGPL version 3"

#define OT_MSG_BACKEND_AUTO       "automatic"
#define OT_MSG_BACKEND_LEGACY     "ASPITAPE-compatible"
#define OT_MSG_BACKEND_OPENASPI   "OpenASPI"
#define OT_MSG_BACKEND_DIRECT     "direct ASPI"
#define OT_MSG_BACKEND_NONE       "none"

#endif

#endif

#ifndef OPENTAPE_MESSAGES_H
#define OPENTAPE_MESSAGES_H

/* Open386MAX-compatible build languages: US (default) and GR (German). */
#ifdef LANG_GR

#define OT_MSG_APP_TITLE          "OpenTape"
#define OT_MSG_VERSION            "OpenTape 0.1 Startversion"
#define OT_MSG_USAGE              "Aufruf: OPENTAPE [--help]"
#define OT_MSG_UNKNOWN_OPTION     "OpenTape: unbekannte Option"
#define OT_MSG_TASK_SUMMARY       "Sichern, wiederherstellen, pruefen, katalogisieren"
#define OT_MSG_TASK_SUMMARY_LONG  "Sicherung und Wiederherstellung fuer SCSI-Band"
#define OT_MSG_NOT_IMPLEMENTED    "Bandzugriff ist noch nicht implementiert."
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
#define OT_MSG_USAGE              "Usage: OPENTAPE [--help]"
#define OT_MSG_UNKNOWN_OPTION     "OpenTape: unknown option"
#define OT_MSG_TASK_SUMMARY       "Backup, restore, verify and catalogue"
#define OT_MSG_TASK_SUMMARY_LONG  "Backup, restore, verify and catalogue for SCSI tape"
#define OT_MSG_NOT_IMPLEMENTED    "Tape access is not implemented yet."
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

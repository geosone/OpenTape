# Shared user-interface language

OpenTape follows the Open386MAX interface language. The DOS frontend links the
original QUILIB, and the GeosOne DOS Installer already follows the same visual
scheme. OpenASPI utilities should use these rules as well.

- Use the QUILIB palette: dark background, blue data area, grey quick-help
  area, cyan dialogs, red error dialogs and yellow highlights.
- Keep the classic 80x25 layout usable and preserve monochrome support.
- Put the product name at the top, context help at the right, and stable
  function-key or Escape actions in the bottom key bar.
- Reuse QUILIB menu, dialog, input, message, keyboard and mouse behaviour;
  do not create project-local replacements.
- Use the same task names on DOS and Windows: Backup, Restore, Verify,
  Catalogue, Media and Settings. Keep destructive actions explicit.
- Keep all visible application text in `include/opentape/messages.h`. As in
  Open386MAX, `MAXLANG=US` selects English and `MAXLANG=GR` selects German;
  DOS and Windows must always use the same selected table.
- Keep command-line operation available for scripts and unattended backups.
- Use GDI for installation and removal so driver and program packages share
  the same setup vocabulary and appearance.

The single Windows frontend uses the existing wxWindows 1.68E Win16 port and
runs on Windows 3.1 and Windows 95/98/ME. It keeps the same task model,
wording, ordering and keyboard shortcuts as DOS. Platform frontends call the
same application services and never implement tape or archive logic.

QUILIB remains part of the `deps/Open386MAX` submodule for now. `OPEN386MAX`
is the only DOS build location contract, allowing that submodule to be replaced
by a future standalone QUILIB repository without changing OpenTape's
application UI.

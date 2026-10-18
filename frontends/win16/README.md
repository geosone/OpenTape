# Windows 3.1 frontend

This is the one shared Windows frontend. It will use the existing
OpenWatcom-compatible wxWindows 1.68E library from the sibling `wxWindows`
project. It targets Win16 and therefore runs on Windows 3.1 as well as Windows
95/98/ME.

Only presentation and Windows-specific device adaptation belong here. Backup,
restore, verify, catalogue and media policy stay in the shared core.

The current `OTAPEWIN.EXE` is a start screen that exposes the shared task
names and backend selection. The backup actions are not wired to application
services yet.

# Portable Windows distribution

Build Release first, then run from the project root:

```powershell
./tools/package_friends.ps1 | Set-Content -Encoding utf8 out/friends-package-path.txt
python tools/finalize_friends_package.py
```

The scripts collect the game/server, five runtime archives, default settings,
required DLLs, font/library notices, and a file hash manifest. The ZIP is written
to `out/releases/`. Only HD graphics and the runtime TTF are packaged.

Give friends the complete ZIP and have them extract it before starting
`Start-Blobby.cmd`. No installer is needed. Windows target: Windows 10/11 x64.
Preferences are stored separately in the user's SDL/PhysFS preference directory.

Preserve `COPYING` and third-party notices. When distributing the game, make the
corresponding source available under its GPL terms. This repository provides the
source; use the revision that matches the distributed binary.

# Allclient installer

`Allclient.iss` builds one graphical Windows installer that:

1. accepts either the rotating Google Apps Script code or the built-in offline code;
2. installs the complete game;
3. updates SmartEmu configuration for the chosen installation path;
4. creates one Allclient desktop shortcut with automatic microphone detection;
5. creates a standard Windows uninstaller.

`Allclient.exe` probes the default Windows capture endpoint on every launch,
without recording audio. A usable endpoint enables voice; an unavailable,
disabled or inaccessible endpoint selects no voice. It updates the existing
SmartEmu profile and starts SSELauncher, which starts `cstrike.exe`. No second
emulator folder is required. Reconnect the microphone and restart the game to
re-evaluate the mode. `Allclient.exe --probe-microphone` performs only the probe
(exit code 0 = available, 1 = unavailable).

Regression checks (isolated profile and emulator stub; does not start the game):

```powershell
rtk proxy cmake --build build/vs2022 --config Release --target auto_launcher auto_launcher_tests
rtk proxy out/bin/Release/auto_launcher_tests.exe
```

Online verification uses the first-party endpoint at
`http://gameland.cam/installer_access.php`. On Windows 7 it first uses URLMon,
which shares Internet Explorer's working WinINet and proxy configuration.
It then tries Inno Setup's downloader and Windows WinHTTP. If those transports
cannot complete the request, the access page reports the connection failure and
offers a manual retry. No browser executable is bundled or required. The access
page includes a connection-status indicator and a manual refresh button.

## Supported Windows versions

- Windows 7 SP1 (x86 and x64), Windows 8/8.1, Windows 10 and Windows 11 are
  supported by the normal VS2022 build and this installer.
- Windows XP is not supported by this build. VS2022, Inno Setup 7 and several
  runtime APIs in the client cannot run on XP. An XP release would
  require a separate `v141_xp` toolchain, older dependencies, a different
  installer and a dedicated test/release pipeline; changing `WINVER` alone is
  not sufficient.

## Build

Install Inno Setup 7, then run from the repository root. This builds and deploys
the current client before packaging and verifies every deployed DLL against the
build output. It also writes a SHA-256 manifest beside the installer:

```powershell
rtk proxy powershell -NoProfile -ExecutionPolicy Bypass -File .\installer\Build-Installer.ps1
```

The default game source is `F:\CS 1.6 - AllClient`. To use another clean source folder:

```powershell
rtk proxy powershell -NoProfile -ExecutionPolicy Bypass -File .\installer\Build-Installer.ps1 -SourceRoot "F:\Path\To\Allclient"
```

The compiled installer is written to `installer\output\Allclient-Setup.exe`.
The configured `NEXTCLIENT_INSTALL_DIR` must match `SourceRoot`. Running ISCC
directly only repackages the files already in that directory; it does not build
or deploy connection fixes from this repository.

Before building, deploy `hosting/installer_access.php` and the management panel
to the first-party host, then generate an active installation code.

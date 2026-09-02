# Allclient installer

`Allclient.iss` builds one graphical Windows installer that:

1. accepts either the rotating Google Apps Script code or the built-in offline code;
2. installs the complete game;
3. updates both SmartEmu configuration files for the chosen installation path;
4. creates Voice and No Voice desktop shortcuts;
5. creates a standard Windows uninstaller.

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

Install Inno Setup 7, then run from the repository root:

```powershell
& "C:\Program Files\Inno Setup 7\ISCC.exe" ".\installer\Allclient.iss"
```

The default game source is `F:\CS 1.6 - AllClient`. To use another clean source folder:

```powershell
& "C:\Program Files\Inno Setup 7\ISCC.exe" "/DSourceRoot=F:\Path\To\Allclient" ".\installer\Allclient.iss"
```

The compiled installer is written to `installer\output\Allclient-Setup.exe`.

Before building, deploy `hosting/installer_access.php` and the management panel
to the first-party host, then generate an active installation code.

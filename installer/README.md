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
cannot complete the request, the installer automatically falls back to bundled
Chromium 109 x86 in hidden headless mode.
Chromium is extracted into the installer's temporary directory and used only
for access requests; an installed browser is not required. The access page
includes a connection-status indicator and a manual refresh button.

## Supported Windows versions

- Windows 7 SP1 (x86 and x64), Windows 8/8.1, Windows 10 and Windows 11 are
  supported by the normal VS2022 build and this installer.
- Windows XP is not supported by this build. VS2022, Inno Setup 7, Chromium 109
  and several runtime APIs in the client cannot run on XP. An XP release would
  require a separate `v141_xp` toolchain, older dependencies, a different
  installer and a dedicated test/release pipeline; changing `WINVER` alone is
  not sufficient.

## Chromium 109 fallback

Place the portable x86 archive at:

```text
installer\runtime\chromium109\chrome.nosync.7z
```

The archive must contain `chrome.exe`; its internal folder layout does not
matter. The build requires the archive and stops with a clear compiler error if
it is missing. The archive is stored without a second compression pass, then
extracted on demand when the online service is first contacted.

Chromium 109 is required only for legacy Windows 7/8 compatibility. It is no
longer security-maintained, so it is launched headlessly with a temporary profile
and is not installed as the user's general-purpose browser.

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

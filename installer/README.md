# Allclient installer

`Allclient.iss` builds one graphical Windows installer that:

1. accepts either the rotating Google Apps Script code or the built-in offline code;
2. installs the complete game;
3. updates both SmartEmu configuration files for the chosen installation path;
4. creates Voice and No Voice desktop shortcuts;
5. creates a standard Windows uninstaller.

Online verification first uses an installed Chrome or Edge browser in hidden
headless mode, which avoids networks where the native Windows downloader cannot
complete a Google TLS connection. If neither browser is installed, the installer
extracts its bundled Chromium 109 x86 into its temporary directory and uses it
only for the access request. The access page includes a connection-status
indicator and a manual refresh button. A built-in HTTPS fallback remains as the
last attempt.

## Chromium 109 fallback

Place the portable x86 archive at:

```text
installer\runtime\chromium109\chrome.nosync.7z
```

The archive must contain `chrome.exe`; its internal folder layout does not
matter. The build automatically includes the archive when it exists and omits
the fallback when it does not. The archive is stored without a second compression
pass, then extracted on demand only if Chrome and Edge are both unavailable.

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

Before building, deploy `google_apps_script\Code.gs` by following its README,
then replace `PASTE_GOOGLE_APPS_SCRIPT_WEB_APP_URL_HERE` in `Allclient.iss` with
the deployed URL ending in `/exec`.

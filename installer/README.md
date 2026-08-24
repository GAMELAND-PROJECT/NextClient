# Allclient installer

`Allclient.iss` builds one graphical Windows installer that:

1. accepts either the rotating Google Apps Script code or the built-in offline code;
2. installs the complete game;
3. updates both SmartEmu configuration files for the chosen installation path;
4. creates Voice and No Voice desktop shortcuts;
5. creates a standard Windows uninstaller.

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

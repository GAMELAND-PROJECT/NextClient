[Setup]
AppName=GAMELAND CS 1.6
AppVersion=1.0
DefaultDirName={pf}\GAMELAND CS 1.6
DefaultGroupName=GAMELAND
OutputDir=Output
OutputBaseFilename=GAMELAND_Setup
Compression=lzma2/ultra
SolidCompression=yes
SetupIconFile=compiler:SetupClassicIcon.ico
UninstallDisplayIcon={app}\Allclient.exe

[Files]
; Copy everything from the compiled output folder or the user's test folder.
; Replace this path with the actual game files directory if needed.
Source: "F:\CS 1.6 - AllClient\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs overwritereadonly

[Registry]
; This will be populated dynamically by the Pascal script below

[Icons]
Name: "{group}\GAMELAND CS 1.6"; Filename: "{app}\Allclient.exe"
Name: "{commondesktop}\GAMELAND CS 1.6"; Filename: "{app}\Allclient.exe"

[Code]
#ifdef UNICODE
  #define AW "W"
#else
  #define AW "A"
#endif

function GetVolumeInformation(
  lpRootPathName: String;
  lpVolumeNameBuffer: String;
  nVolumeNameSize: DWORD;
  var lpVolumeSerialNumber: DWORD;
  var lpMaximumComponentLength: DWORD;
  var lpFileSystemFlags: DWORD;
  lpFileSystemNameBuffer: String;
  nFileSystemNameSize: DWORD
): BOOL;
external 'GetVolumeInformation{#AW}@kernel32.dll stdcall';

function GetHWID(): String;
var
  SerialNum: DWORD;
  Dummy: DWORD;
begin
  SerialNum := 0;
  if GetVolumeInformation('C:\', '', 0, SerialNum, Dummy, Dummy, '', 0) then
  begin
    Result := Format('%.8X', [SerialNum]);
  end
  else
  begin
    Result := 'UNKNOWN_HWID';
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Write the Hardware ID (Volume Serial Number) to the registry
    // This allows AntiCopy.cpp to verify that the game is running on the machine it was installed on.
    RegWriteStringValue(HKEY_CURRENT_USER, 'Software\NextClient', 'InstallID', GetHWID());
  end;
end;

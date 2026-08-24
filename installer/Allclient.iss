#define AppName "Allclient"
#define AppVersion "2.5.3"
#define AppPublisher "GAMELAND PROJECT"
#define AppExeName "cstrike.exe"

#ifndef SourceRoot
  #define SourceRoot "F:\CS 1.6 - AllClient"
#endif

[Setup]
AppId={{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Allclient
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=Allclient-Setup
SetupIconFile=..\nextclient\launcher\src\next_launcher\assets\app_icon.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no
SetupLogging=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "crashes\*,htmlcache\*,*.log,*.mdmp,debug.log,install.bat,unins000.exe,unins000.dat"

[Icons]
Name: "{autodesktop}\Allclient - Voice Enabled"; Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; IconFilename: "{app}\cstrike.exe"
Name: "{autodesktop}\Allclient - No Voice"; Filename: "{app}\platform\steam\games\SmartEmu2\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu2"; IconFilename: "{app}\cstrike.exe"
Name: "{group}\Uninstall Allclient"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; Description: "Launch Allclient (Voice Enabled)"; Flags: nowait postinstall skipifsilent unchecked

[Code]
const
  { Replace this value with the /exec URL created by Google Apps Script. }
  AccessApiUrl = 'PASTE_GOOGLE_APPS_SCRIPT_WEB_APP_URL_HERE';
  OfflineCode = 'amir1394';

var
  AccessPage: TInputQueryWizardPage;

function OnlineAccessCodeIsValid(const EnteredCode: String): Boolean;
var
  RequestUrl, ResponsePath: String;
  ResponseText: AnsiString;
begin
  Result := False;
  if Pos('PASTE_', AccessApiUrl) = 1 then
  begin
    Log('Google Apps Script URL has not been configured.');
    Exit;
  end;

  RequestUrl := AccessApiUrl + '?action=verify&code=' + Trim(EnteredCode);
  ResponsePath := ExpandConstant('{tmp}\allclient-verify.json');
  DeleteFile(ResponsePath);

  try
    DownloadTemporaryFile(RequestUrl, 'allclient-verify.json', '', nil);
    if LoadStringFromFile(ResponsePath, ResponseText) then
      Result := Pos('"valid":true', Lowercase(ResponseText)) > 0;
  except
    Log('Online code verification unavailable: ' + GetExceptionMessage);
    Result := False;
  end;
end;

function AccessCodeIsValid(const EnteredCode: String): Boolean;
begin
  Result := CompareText(Trim(EnteredCode), OfflineCode) = 0;
  if not Result then
    Result := OnlineAccessCodeIsValid(EnteredCode);
end;

procedure InitializeWizard;
begin
  AccessPage := CreateInputQueryPage(
    wpWelcome,
    'Installation access',
    'Enter your Allclient installation code',
    'Enter the active 8-digit online code or the offline recovery code.');
  AccessPage.Add('Access code:', True);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = AccessPage.ID then
  begin
    Result := AccessCodeIsValid(AccessPage.Values[0]);
    if not Result then
    begin
      MsgBox('The installation code is invalid.', mbError, MB_OK);
      WizardForm.ActiveControl := AccessPage.Edits[0];
    end;
  end;
end;

procedure SetXmlElement(const FileName, ElementName, NewValue: String);
var
  Content, OpenTag, CloseTag, Tail: AnsiString;
  ValueStart, CloseRelative: Integer;
begin
  if not LoadStringFromFile(FileName, Content) then
    RaiseException('Unable to read configuration file: ' + FileName);

  OpenTag := '<' + ElementName + '>';
  CloseTag := '</' + ElementName + '>';
  ValueStart := Pos(OpenTag, Content);
  if ValueStart = 0 then
    RaiseException('Missing XML element ' + ElementName + ' in ' + FileName);

  ValueStart := ValueStart + Length(OpenTag);
  Tail := Copy(Content, ValueStart, MaxInt);
  CloseRelative := Pos(CloseTag, Tail);
  if CloseRelative = 0 then
    RaiseException('Invalid XML element ' + ElementName + ' in ' + FileName);

  Delete(Content, ValueStart, CloseRelative - 1);
  Insert(NewValue, Content, ValueStart);
  if not SaveStringToFile(FileName, Content, False) then
    RaiseException('Unable to update configuration file: ' + FileName);
end;

procedure ConfigureSmartEmu(const ConfigFile: String);
begin
  SetXmlElement(ConfigFile, 'Path', ExpandConstant('{app}\cstrike.exe'));
  SetXmlElement(ConfigFile, 'StartIn', ExpandConstant('{app}'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ConfigureSmartEmu(ExpandConstant('{app}\platform\steam\games\SmartEmu\config.xml'));
    ConfigureSmartEmu(ExpandConstant('{app}\platform\steam\games\SmartEmu2\config.xml'));
  end;
end;

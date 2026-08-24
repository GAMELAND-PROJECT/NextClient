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
  AccessManifestUrl = 'https://raw.githubusercontent.com/GAMELAND-PROJECT/NextClient/refs/heads/%D8%B4%D8%B1%D9%88%D8%B9-%D9%82%D8%B3%D9%85%D8%AA-%DA%A9%D9%84%D8%A7%DB%8C%D9%86%D8%AA-%D8%A8%D8%B1%D8%A7%DB%8C-%D8%A8%D9%82%DB%8C%D9%87-%D9%BE%DB%8C%D9%85%D9%86%D8%AA-%D9%87%D8%A7/installer_access.json';
  OfflineCode = 'amir1394';
  BuiltInOnlineCode = 'NCL-A1FC6BC1-84EB6B58-AF65B795';

var
  AccessPage: TInputQueryWizardPage;
  OnlineCode: String;
  ManifestChecked: Boolean;

function JsonStringValue(const Json, Key: String): String;
var
  KeyPos, ColonPos, StartPos, EndPos: Integer;
  Tail: String;
begin
  Result := '';
  KeyPos := Pos('"' + Key + '"', Json);
  if KeyPos = 0 then
    Exit;

  Tail := Copy(Json, KeyPos + Length(Key) + 2, MaxInt);
  ColonPos := Pos(':', Tail);
  if ColonPos = 0 then
    Exit;

  Tail := Copy(Tail, ColonPos + 1, MaxInt);
  StartPos := Pos('"', Tail);
  if StartPos = 0 then
    Exit;

  Tail := Copy(Tail, StartPos + 1, MaxInt);
  EndPos := Pos('"', Tail);
  if EndPos = 0 then
    Exit;

  Result := Copy(Tail, 1, EndPos - 1);
end;

function JsonEnabled(const Json: String): Boolean;
var
  CompactJson: String;
begin
  CompactJson := Lowercase(Json);
  StringChangeEx(CompactJson, ' ', '', True);
  StringChangeEx(CompactJson, #13, '', True);
  StringChangeEx(CompactJson, #10, '', True);
  StringChangeEx(CompactJson, #9, '', True);
  Result := Pos('"enabled":true', CompactJson) > 0;
end;

procedure RefreshOnlineCode;
var
  ManifestPath, ManifestText, DownloadedCode: String;
begin
  if ManifestChecked then
    Exit;

  ManifestChecked := True;
  OnlineCode := BuiltInOnlineCode;
  ManifestPath := ExpandConstant('{tmp}\allclient-access.json');

  try
    DownloadTemporaryFile(AccessManifestUrl, 'allclient-access.json', '', nil);
    if LoadStringFromFile(ManifestPath, ManifestText) then
    begin
      if JsonEnabled(ManifestText) then
      begin
        DownloadedCode := JsonStringValue(ManifestText, 'access_code');
        if DownloadedCode <> '' then
          OnlineCode := DownloadedCode;
      end
      else
        OnlineCode := '';
    end;
  except
    Log('Online access manifest unavailable: ' + GetExceptionMessage);
  end;
end;

function AccessCodeIsValid(const EnteredCode: String): Boolean;
begin
  RefreshOnlineCode;
  Result := CompareText(Trim(EnteredCode), OfflineCode) = 0;
  if (not Result) and (OnlineCode <> '') then
    Result := CompareText(Trim(EnteredCode), OnlineCode) = 0;
end;

procedure InitializeWizard;
begin
  OnlineCode := BuiltInOnlineCode;
  ManifestChecked := False;
  AccessPage := CreateInputQueryPage(
    wpWelcome,
    'Installation access',
    'Enter your Allclient installation code',
    'Both the online code and the offline recovery code are accepted.');
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
      AccessPage.Edits[0].SetFocus;
    end;
  end;
end;

procedure SetXmlElement(const FileName, ElementName, NewValue: String);
var
  Content, OpenTag, CloseTag, Tail: String;
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


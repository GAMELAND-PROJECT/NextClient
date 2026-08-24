#define AppName "Allclient"
#define AppVersion "2.5.3"
#define AppPublisher "GAMELAND PROJECT"
#define AppExeName "cstrike.exe"

#ifndef SourceRoot
  #define SourceRoot "F:\CS 1.6 - AllClient"
#endif

#define BundledChromiumArchive "runtime\chromium109\chrome.nosync.7z"
#if FileExists(BundledChromiumArchive)
  #define HasBundledChromium
#endif

[Setup]
AppId={{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Allclient
DefaultGroupName={#AppName}
DisableDirPage=no
UsePreviousAppDir=no
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
ArchiveExtraction=enhanced/nopassword

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
#ifdef HasBundledChromium
Source: "{#BundledChromiumArchive}"; Flags: dontcopy noencryption nocompression
#endif
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "crashes\*,htmlcache\*,*.log,*.mdmp,debug.log,install.bat,unins000.exe,unins000.dat"

[Icons]
Name: "{autodesktop}\Allclient - Voice Enabled"; Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; IconFilename: "{app}\cstrike.exe"
Name: "{autodesktop}\Allclient - No Voice"; Filename: "{app}\platform\steam\games\SmartEmu2\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu2"; IconFilename: "{app}\cstrike.exe"
Name: "{group}\Uninstall Allclient"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; Description: "Launch Allclient (Voice Enabled)"; Flags: nowait postinstall skipifsilent unchecked

[Code]
const
  AccessApiUrl = 'https://script.google.com/macros/s/AKfycbyPOh-kYDfyANzHK3_D6L1_W9Km-RT1oqGp3makI6T-Z97ksgsPZXug2ld18nF3vi1X/exec';
  OfflineCode = 'amir1394';

var
  AccessPage: TInputQueryWizardPage;
  AccessStatusLabel: TNewStaticText;
  RefreshAccessButton: TNewButton;
  OnlineServiceUnavailable: Boolean;
  BundledChromiumPath: String;

function FindChromeExecutable(const Directory: String; var ChromePath: String): Boolean;
var
  FindRec: TFindRec;
  Candidate: String;
begin
  Result := False;
  Candidate := AddBackslash(Directory) + 'chrome.exe';
  if FileExists(Candidate) then
  begin
    ChromePath := Candidate;
    Result := True;
    Exit;
  end;

  if FindFirst(AddBackslash(Directory) + '*', FindRec) then
  begin
    try
      repeat
        if ((FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and
           (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          if FindChromeExecutable(AddBackslash(Directory) + FindRec.Name, ChromePath) then
          begin
            Result := True;
            Exit;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

function PrepareBundledChromium(var BrowserPath: String): Boolean;
var
  ArchivePath, ExtractPath: String;
begin
  Result := False;
  BrowserPath := '';

#ifdef HasBundledChromium
  if (BundledChromiumPath <> '') and FileExists(BundledChromiumPath) then
  begin
    BrowserPath := BundledChromiumPath;
    Result := True;
    Exit;
  end;

  ArchivePath := ExpandConstant('{tmp}\chrome.nosync.7z');
  ExtractPath := ExpandConstant('{tmp}\allclient-chromium109');

  try
    if not FileExists(ArchivePath) then
      ExtractTemporaryFile('chrome.nosync.7z');

    if not DirExists(ExtractPath) then
    begin
      ForceDirectories(ExtractPath);
      ExtractArchive(ArchivePath, ExtractPath, '', True, nil);
    end;

    if FindChromeExecutable(ExtractPath, BundledChromiumPath) then
    begin
      BrowserPath := BundledChromiumPath;
      Result := True;
    end;
  except
    Log('Bundled Chromium extraction failed: ' + GetExceptionMessage);
  end;
#endif
end;

function FindChromiumBrowser(var BrowserPath: String): Boolean;
begin
  Result := False;
  BrowserPath := '';

  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe', '', BrowserPath) and FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  if RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe', '', BrowserPath) and FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and RegQueryStringValue(HKLM64, 'Software\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe', '', BrowserPath) and FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  if RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe', '', BrowserPath) and FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and RegQueryStringValue(HKLM64, 'Software\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe', '', BrowserPath) and FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;

  BrowserPath := ExpandConstant('{localappdata}\Google\Chrome\Application\chrome.exe');
  if FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  BrowserPath := ExpandConstant('{pf}\Google\Chrome\Application\chrome.exe');
  if FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;
  BrowserPath := ExpandConstant('{pf32}\Microsoft\Edge\Application\msedge.exe');
  if FileExists(BrowserPath) then
  begin
    Result := True;
    Exit;
  end;

  { The bundled Chromium is extracted only when no installed browser exists. }
  Result := PrepareBundledChromium(BrowserPath);
end;

function DownloadWithChromium(const Url, ResponsePath: String): Boolean;
var
  BrowserPath, ScriptPath, ProfilePath, Parameters: String;
  CommandText: AnsiString;
  ResultCode: Integer;
begin
  Result := False;
  if not FindChromiumBrowser(BrowserPath) then
    Exit;

  ScriptPath := ExpandConstant('{tmp}\allclient-browser-request.cmd');
  ProfilePath := ExpandConstant('{tmp}\allclient-browser-profile');
  DeleteFile(ResponsePath);
  DelTree(ProfilePath, True, True, True);

  CommandText := '@echo off' + #13#10 +
    '"' + BrowserPath + '" --headless=new --disable-gpu --no-first-run ' +
    '--disable-background-networking --user-data-dir="' + ProfilePath + '" ' +
    '--dump-dom "' + Url + '" > "' + ResponsePath + '" 2>nul' + #13#10;

  if not SaveStringToFile(ScriptPath, CommandText, False) then
    Exit;

  Parameters := '/D /C ""' + ScriptPath + '""';
  if Exec(ExpandConstant('{cmd}'), Parameters, ExpandConstant('{tmp}'), SW_HIDE,
      ewWaitUntilTerminated, ResultCode) then
    Result := (ResultCode = 0) and FileExists(ResponsePath);

  DeleteFile(ScriptPath);
  DelTree(ProfilePath, True, True, True);
end;

function FetchAccessResponse(const Url: String; var ResponseText: AnsiString): Boolean;
var
  ResponsePath: String;
begin
  Result := False;
  ResponseText := '';
  ResponsePath := ExpandConstant('{tmp}\allclient-access-response.html');

  if DownloadWithChromium(Url, ResponsePath) then
  begin
    Result := LoadStringFromFile(ResponsePath, ResponseText);
    Exit;
  end;

  { Fallback for systems without Chrome or Edge. }
  try
    ResponsePath := ExpandConstant('{tmp}\allclient-access-response.json');
    DeleteFile(ResponsePath);
    DownloadTemporaryFile(Url, 'allclient-access-response.json', '', nil);
    Result := LoadStringFromFile(ResponsePath, ResponseText);
  except
    Log('Built-in access download failed: ' + GetExceptionMessage);
    Result := False;
  end;
end;

function OnlineAccessCodeIsValid(const EnteredCode: String): Boolean;
var
  RequestUrl: String;
  ResponseText: AnsiString;
begin
  Result := False;
  OnlineServiceUnavailable := False;
  if Pos('PASTE_', AccessApiUrl) = 1 then
  begin
    Log('Google Apps Script URL has not been configured.');
    OnlineServiceUnavailable := True;
    Exit;
  end;

  RequestUrl := AccessApiUrl + '?action=verify&code=' + Trim(EnteredCode);
  if FetchAccessResponse(RequestUrl, ResponseText) then
    Result := Pos('"valid":true', Lowercase(ResponseText)) > 0
  else
  begin
    OnlineServiceUnavailable := True;
    Log('Online code verification unavailable through Chromium and built-in downloader.');
  end;
end;

function AccessCodeIsValid(const EnteredCode: String): Boolean;
begin
  Result := CompareText(Trim(EnteredCode), OfflineCode) = 0;
  if not Result then
    Result := OnlineAccessCodeIsValid(EnteredCode);
end;

procedure RefreshAccessStatus(Sender: TObject);
var
  ResponseText: AnsiString;
begin
  RefreshAccessButton.Enabled := False;
  AccessStatusLabel.Font.Color := clGray;
  AccessStatusLabel.Caption := 'Checking online service...';

  if FetchAccessResponse(AccessApiUrl, ResponseText) and
     (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
  begin
    AccessStatusLabel.Font.Color := clGreen;
    AccessStatusLabel.Caption := 'Online service: connected';
  end
  else
  begin
    AccessStatusLabel.Font.Color := clRed;
    AccessStatusLabel.Caption := 'Online service: unavailable (offline code still works)';
  end;
  RefreshAccessButton.Enabled := True;
end;

procedure InitializeWizard;
begin
  AccessPage := CreateInputQueryPage(
    wpWelcome,
    'Installation access',
    'Enter your Allclient installation code',
    'Enter the active 8-digit online code or the offline recovery code.');
  AccessPage.Add('Access code:', True);

  AccessStatusLabel := TNewStaticText.Create(WizardForm);
  AccessStatusLabel.Parent := AccessPage.Surface;
  AccessStatusLabel.Left := AccessPage.Edits[0].Left;
  AccessStatusLabel.Top := AccessPage.Edits[0].Top + AccessPage.Edits[0].Height + ScaleY(20);
  AccessStatusLabel.Width := AccessPage.SurfaceWidth;
  AccessStatusLabel.Caption := 'Online service: not checked';
  AccessStatusLabel.Font.Color := clGray;

  RefreshAccessButton := TNewButton.Create(WizardForm);
  RefreshAccessButton.Parent := AccessPage.Surface;
  RefreshAccessButton.Left := AccessPage.Edits[0].Left;
  RefreshAccessButton.Top := AccessStatusLabel.Top + AccessStatusLabel.Height + ScaleY(10);
  RefreshAccessButton.Width := ScaleX(150);
  RefreshAccessButton.Height := ScaleY(30);
  RefreshAccessButton.Caption := 'Refresh connection';
  RefreshAccessButton.OnClick := @RefreshAccessStatus;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = AccessPage.ID then
  begin
    Result := AccessCodeIsValid(AccessPage.Values[0]);
    if not Result then
    begin
      if OnlineServiceUnavailable then
        MsgBox('Online verification is unavailable. Check the Internet connection or use the offline recovery code.', mbError, MB_OK)
      else
        MsgBox('The installation code is invalid or has been revoked.', mbError, MB_OK);
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

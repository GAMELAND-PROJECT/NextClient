#define AppName "Allclient"
#define AppVersion "2.5.3"
#define AppPublisher "GAMELAND PROJECT"
#define AppExeName "cstrike.exe"

#ifndef SourceRoot
  #define SourceRoot "F:\CS 1.6 - AllClient"
#endif

#define BundledChromiumArchive AddBackslash(SourcePath) + "runtime\chromium109\chrome.nosync.7z"
#ifnexist BundledChromiumArchive
  #error Bundled Chromium is missing: installer\runtime\chromium109\chrome.nosync.7z
#endif
#define HasBundledChromium

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
  PreparationPage: TWizardPage;
  PreparationStatusLabel: TNewStaticText;
  PreparationProgress: TNewProgressBar;
  PreparationRetryButton: TNewButton;
  PreparationStarted: Boolean;
  PreparationReady: Boolean;
  AccessPage: TInputQueryWizardPage;
  AccessStatusLabel: TNewStaticText;
  RefreshAccessButton: TNewButton;
  OnlineServiceUnavailable: Boolean;
  BundledChromiumPath: String;
  OnlineVerificationMessage: String;

procedure SetAccessStatus(const Caption: String; Color: TColor);
begin
  AccessStatusLabel.Font.Color := Color;
  AccessStatusLabel.Caption := Caption;
  WizardForm.Update;
end;

function IsEightDigitCode(const Value: String): Boolean;
var
  I: Integer;
  Code: String;
begin
  Code := Trim(Value);
  Result := Length(Code) = 8;
  if Result then
    for I := 1 to Length(Code) do
      if (Code[I] < '0') or (Code[I] > '9') then
      begin
        Result := False;
        Exit;
      end;
end;

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
    { Keep internal paths and exception details out of installer output. }
    Result := False;
  end;
#endif
end;

function FetchWithChromium(const Url: String; var ResponseText: AnsiString): Boolean;
var
  BrowserPath, ProfilePath, Parameters: String;
  ResultCode, I: Integer;
  Output: TExecOutput;
begin
  Result := False;
  ResponseText := '';
  ProfilePath := ExpandConstant('{tmp}\allclient-browser-profile');
  try
    try
      { Online access always uses the Chromium runtime embedded in this Setup. }
      if not PrepareBundledChromium(BrowserPath) then
      begin
        OnlineVerificationMessage := 'Verification component could not be prepared.';
        Exit;
      end;

      SetAccessStatus('Verification component ready. Contacting online service...', clGray);
      DelTree(ProfilePath, True, True, True);

      Parameters := '--headless --disable-gpu --no-first-run ' +
      '--no-default-browser-check --disable-extensions --disable-sync ' +
      '--disable-component-update --disable-background-networking ' +
      '--disable-crash-reporter --disable-breakpad --no-pings ' +
      '--disable-default-apps --disable-logging --disable-metrics ' +
      '--disable-translate --dns-prefetch-disable --disable-preconnect ' +
      '--safebrowsing-disable-auto-update ' +
      '--disable-client-side-phishing-detection ' +
      '--blink-settings=imagesEnabled=false ' +
      '--disable-features=MediaRouter,OptimizationHints,Translate,' +
      'AutofillServerCommunication,CertificateTransparencyComponentUpdater ' +
      '--virtual-time-budget=60000 ' +
      '--user-data-dir="' + ProfilePath + '" ' +
      '--dump-dom "' + Url + '"';

      if not ExecAndCaptureOutput(BrowserPath, Parameters, ExpandConstant('{tmp}'),
          SW_HIDE, ewWaitUntilTerminated, ResultCode, Output) then
      begin
        OnlineVerificationMessage := 'Verification component could not be started.';
        Exit;
      end;

      if (ResultCode <> 0) or Output.Error then
      begin
        OnlineVerificationMessage := 'The online request did not complete successfully.';
        Exit;
      end;

      for I := 0 to GetArrayLength(Output.StdOut) - 1 do
      begin
        ResponseText := ResponseText + Output.StdOut[I] + #10;
        if Length(ResponseText) > 262144 then
        begin
          ResponseText := '';
          OnlineVerificationMessage := 'The service returned an invalid response.';
          Exit;
        end;
      end;

      if Length(ResponseText) = 0 then
      begin
        OnlineVerificationMessage := 'No response was received from the online service.';
        Exit;
      end;

      Result := True;
    except
      OnlineVerificationMessage := 'The verification component encountered a recoverable error.';
      Result := False;
    end;
  finally
    DelTree(ProfilePath, True, True, True);
  end;
end;

function FetchAccessResponse(const Url: String; var ResponseText: AnsiString): Boolean;
var
  Attempt: Integer;
begin
  Result := False;
  ResponseText := '';
  for Attempt := 1 to 2 do
  begin
    SetAccessStatus('Connecting to online service (attempt ' +
      IntToStr(Attempt) + ' of 2)...', clGray);
    try
      if FetchWithChromium(Url, ResponseText) then
      begin
        Result := True;
        Exit;
      end
      else if Attempt < 2 then
        SetAccessStatus('Connection was interrupted. Retrying safely...', clGray);
    except
      OnlineVerificationMessage := 'The service response could not be processed safely.';
      Result := False;
    end;
  end;
end;

function OnlineAccessCodeIsValid(const EnteredCode: String): Boolean;
var
  RequestUrl: String;
  ResponseText: AnsiString;
begin
  Result := False;
  OnlineServiceUnavailable := False;
  OnlineVerificationMessage := '';

  if not IsEightDigitCode(EnteredCode) then
  begin
    OnlineVerificationMessage := 'Online codes must contain exactly 8 digits.';
    Exit;
  end;

  if Pos('PASTE_', AccessApiUrl) = 1 then
  begin
    OnlineServiceUnavailable := True;
    OnlineVerificationMessage := 'Online verification is not configured.';
    Exit;
  end;

  SetAccessStatus('Checking the entered code securely...', clGray);
  RequestUrl := AccessApiUrl + '?action=verify&code=' + Trim(EnteredCode);
  if FetchAccessResponse(RequestUrl, ResponseText) then
  begin
    SetAccessStatus('Response received. Validating result...', clGray);
    if Pos('"valid":true', Lowercase(ResponseText)) > 0 then
    begin
      Result := True;
      OnlineVerificationMessage := 'Online code accepted.';
    end
    else if Pos('"valid":false', Lowercase(ResponseText)) > 0 then
      OnlineVerificationMessage := 'The online code is invalid or has been revoked.'
    else
    begin
      OnlineServiceUnavailable := True;
      OnlineVerificationMessage := 'The online service returned an unrecognized response.';
    end;
  end
  else
  begin
    OnlineServiceUnavailable := True;
    if OnlineVerificationMessage = '' then
      OnlineVerificationMessage := 'The online verification service is unavailable.';
  end;
end;

function AccessCodeIsValid(const EnteredCode: String): Boolean;
begin
  Result := CompareText(Trim(EnteredCode), OfflineCode) = 0;
  if Result then
    OnlineVerificationMessage := 'Access code accepted.'
  else
    Result := OnlineAccessCodeIsValid(EnteredCode);
end;

procedure RefreshAccessStatus(Sender: TObject);
var
  ResponseText: AnsiString;
begin
  RefreshAccessButton.Enabled := False;
  OnlineVerificationMessage := '';
  SetAccessStatus('Preparing secure verification component...', clGray);
  try
    if FetchAccessResponse(AccessApiUrl + '?action=status', ResponseText) and
       (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
      SetAccessStatus('Online service: connected and responding normally', clGreen)
    else
    begin
      if OnlineVerificationMessage = '' then
        OnlineVerificationMessage := 'Online service did not return a valid status.';
      SetAccessStatus('Online service unavailable: ' + OnlineVerificationMessage, clRed);
    end;
  except
    SetAccessStatus('Online service unavailable: recoverable verification error', clRed);
  end;
  RefreshAccessButton.Enabled := True;
end;

procedure RunEarlyPreparation(Sender: TObject);
var
  BrowserPath: String;
  ResponseText: AnsiString;
begin
  PreparationStarted := True;
  PreparationReady := False;
  PreparationRetryButton.Visible := False;
  WizardForm.NextButton.Enabled := False;

  PreparationProgress.Position := 15;
  PreparationStatusLabel.Font.Color := clGray;
  PreparationStatusLabel.Caption :=
    'Preparing the secure online verification component...';
  WizardForm.Update;

  try
    if PrepareBundledChromium(BrowserPath) then
    begin
      PreparationProgress.Position := 55;
      PreparationStatusLabel.Caption :=
        'Component prepared. Confirming secure online access...';
      WizardForm.Update;

      if FetchAccessResponse(AccessApiUrl + '?action=status', ResponseText) and
         (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
      begin
        PreparationProgress.Position := 100;
        PreparationReady := True;
        PreparationStatusLabel.Font.Color := clGreen;
        PreparationStatusLabel.Caption :=
          'Configuration complete. Online verification is ready.';
      end
      else
      begin
        PreparationProgress.Position := 55;
        PreparationStatusLabel.Font.Color := clRed;
        PreparationStatusLabel.Caption :=
          'Component is ready, but the online service could not be confirmed. You may retry or continue with offline access.';
        PreparationRetryButton.Visible := True;
      end;
    end
    else
    begin
      PreparationProgress.Position := 0;
      PreparationStatusLabel.Font.Color := clRed;
      PreparationStatusLabel.Caption :=
        'Online verification could not be prepared. You may retry or continue with offline access.';
      PreparationRetryButton.Visible := True;
    end;
  except
    PreparationProgress.Position := 0;
    PreparationStatusLabel.Font.Color := clRed;
    PreparationStatusLabel.Caption :=
      'Preparation stopped safely. You may retry or continue with offline access.';
    PreparationRetryButton.Visible := True;
  end;

  WizardForm.NextButton.Enabled := True;
  WizardForm.Update;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = PreparationPage.ID) and not PreparationStarted then
    RunEarlyPreparation(nil);
end;

procedure InitializeWizard;
begin
  PreparationPage := CreateCustomPage(
    wpWelcome,
    'Preparing Allclient',
    'One-time verification setup');

  PreparationStatusLabel := TNewStaticText.Create(WizardForm);
  PreparationStatusLabel.Parent := PreparationPage.Surface;
  PreparationStatusLabel.Left := ScaleX(12);
  PreparationStatusLabel.Top := ScaleY(34);
  PreparationStatusLabel.Width := PreparationPage.SurfaceWidth - ScaleX(24);
  PreparationStatusLabel.Height := ScaleY(54);
  PreparationStatusLabel.AutoSize := False;
  PreparationStatusLabel.WordWrap := True;
  PreparationStatusLabel.Caption :=
    'Ready to configure the secure online verification component.';
  PreparationStatusLabel.Font.Color := clGray;

  PreparationProgress := TNewProgressBar.Create(WizardForm);
  PreparationProgress.Parent := PreparationPage.Surface;
  PreparationProgress.Left := ScaleX(12);
  PreparationProgress.Top := PreparationStatusLabel.Top +
    PreparationStatusLabel.Height + ScaleY(16);
  PreparationProgress.Width := PreparationPage.SurfaceWidth - ScaleX(24);
  PreparationProgress.Height := ScaleY(18);
  PreparationProgress.Min := 0;
  PreparationProgress.Max := 100;
  PreparationProgress.Position := 0;

  PreparationRetryButton := TNewButton.Create(WizardForm);
  PreparationRetryButton.Parent := PreparationPage.Surface;
  PreparationRetryButton.Left := ScaleX(12);
  PreparationRetryButton.Top := PreparationProgress.Top +
    PreparationProgress.Height + ScaleY(18);
  PreparationRetryButton.Width := ScaleX(130);
  PreparationRetryButton.Height := ScaleY(30);
  PreparationRetryButton.Caption := 'Retry preparation';
  PreparationRetryButton.OnClick := @RunEarlyPreparation;
  PreparationRetryButton.Visible := False;

  AccessPage := CreateInputQueryPage(
    PreparationPage.ID,
    'Installation access',
    'Enter your Allclient installation code',
    'Enter the active 8-digit online code or the offline recovery code.');
  AccessPage.Add('Access code:', True);

  AccessStatusLabel := TNewStaticText.Create(WizardForm);
  AccessStatusLabel.Parent := AccessPage.Surface;
  AccessStatusLabel.Left := AccessPage.Edits[0].Left;
  AccessStatusLabel.Top := AccessPage.Edits[0].Top + AccessPage.Edits[0].Height + ScaleY(20);
  AccessStatusLabel.Width := AccessPage.SurfaceWidth;
  AccessStatusLabel.Height := ScaleY(42);
  AccessStatusLabel.AutoSize := False;
  AccessStatusLabel.WordWrap := True;
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
    WizardForm.NextButton.Enabled := False;
    SetAccessStatus('Validating installation access...', clGray);
    try
      try
        Result := AccessCodeIsValid(AccessPage.Values[0]);
      except
        Result := False;
        OnlineServiceUnavailable := True;
        OnlineVerificationMessage := 'Verification stopped safely after an unexpected error.';
      end;

      if Result then
        SetAccessStatus(OnlineVerificationMessage, clGreen)
      else
      begin
        if OnlineVerificationMessage = '' then
          OnlineVerificationMessage := 'The installation code was not accepted.';
        SetAccessStatus(OnlineVerificationMessage, clRed);
        MsgBox(OnlineVerificationMessage, mbError, MB_OK);
        WizardForm.ActiveControl := AccessPage.Edits[0];
      end;
    finally
      WizardForm.NextButton.Enabled := True;
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

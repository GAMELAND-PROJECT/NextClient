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
; Inno Setup 7 and the application binaries are supported on Windows 7 SP1+
; only. Keep this explicit so an unsupported legacy OS fails before extraction.
MinVersion=6.1sp1
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
  AccessApiUrl = 'https://gameland.cam/installer_access.php';
  OfflineCode = 'amir1394';
  AllclientUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}_is1';

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
  AccessApproved: Boolean;
  PreviousInstallCleanupDone: Boolean;
  PreviousInstallDirectoryPendingCleanup: String;
  DestinationCleanupDone: Boolean;

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

function FetchWithNativeHttps(const Url: String; var ResponseText: String): Boolean;
var
  Request: Variant;
  ProxyMode: Integer;
begin
  Result := False;
  ResponseText := '';
  if Pos('https://', Lowercase(Url)) <> 1 then
  begin
    OnlineVerificationMessage := 'The verification URL is not secure.';
    Exit;
  end;

  { WinHttpRequestOption_SecureProtocols = 9 and TLS 1.2 = 2048.
    This avoids the legacy TLS 1.0 default used by WinHTTP on Windows 7. }
  for ProxyMode := 0 to 1 do
  begin
    try
      Request := CreateOleObject('WinHttp.WinHttpRequest.5.1');
      Request.SetTimeouts(5000, 10000, 10000, 20000);
      if ProxyMode = 1 then
        Request.SetProxy(1); { HTTPREQUEST_PROXYSETTING_DIRECT }
      Request.Option[6] := True; { Follow HTTPS redirects. }
      Request.Option[9] := 2048; { TLS 1.2 only. }
      Request.Open('GET', Url, False);
      Request.SetRequestHeader('User-Agent', 'Allclient-Setup/2.0');
      Request.SetRequestHeader('Cache-Control', 'no-cache');
      Request.Send();

      if Request.Status = 200 then
      begin
        ResponseText := Request.ResponseText;
        if (Length(ResponseText) > 0) and (Length(ResponseText) <= 262144) then
        begin
          Result := True;
          Exit;
        end;
        ResponseText := '';
      end;
    except
      { Try the direct transport and then the bundled Chromium fallback. }
      ResponseText := '';
    end;
  end;
end;

function FetchWithSetupDownloader(const Url: String; var ResponseText: String): Boolean;
var
  DownloadName, DownloadPath: String;
  DownloadedSize: Int64;
  RawResponse: AnsiString;
begin
  Result := False;
  ResponseText := '';
  DownloadName := 'allclient-access-response.tmp';
  DownloadPath := ExpandConstant('{tmp}\') + DownloadName;

  try
    DeleteFile(DownloadPath);
    { Inno Setup's downloader follows redirects and applies the user's proxy
      settings itself. This is the most compatible first transport on Win7. }
    DownloadedSize := DownloadTemporaryFile(Url, DownloadName, '', nil);
    if (DownloadedSize <= 0) or (DownloadedSize > 262144) then
      Exit;

    if not LoadStringFromFile(DownloadPath, RawResponse) then
      Exit;
    if (Length(RawResponse) = 0) or (Length(RawResponse) > 262144) then
      Exit;

    ResponseText := RawResponse;
    Result := True;
  except
    ResponseText := '';
    Result := False;
  end;

  DeleteFile(DownloadPath);
end;

function FetchWithChromium(const Url: String; var ResponseText: String): Boolean;
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
      { Bundled Chromium is the compatibility fallback when native WinHTTP
        cannot complete the secure request on an older Windows installation. }
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
      '--disable-quic --ssl-version-min=tls1.2 ' +
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
          SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode, Output) then
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

function FetchAccessResponse(const Url: String; var ResponseText: String): Boolean;
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
      if FetchWithSetupDownloader(Url, ResponseText) then
      begin
        Result := True;
        Exit;
      end
      else
      begin
        SetAccessStatus('Primary connection unavailable. Trying Windows compatibility mode...', clGray);
        if FetchWithNativeHttps(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;

        SetAccessStatus('Windows connection unavailable. Starting independent compatibility mode...', clGray);
        if FetchWithChromium(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;
      end;

      if Attempt < 2 then
        SetAccessStatus('Connection was interrupted. Retrying safely...', clGray);
    except
      OnlineVerificationMessage := 'The service response could not be processed safely.';
      Result := False;
    end;
  end;
end;

function FetchAccessApi(const Query: String; var ResponseText: String): Boolean;
var
  NormalizedResponse: String;
begin
  SetAccessStatus('Connecting to the Allclient access service...', clGray);
  Result := FetchAccessResponse(AccessApiUrl + Query, ResponseText);
  if Result then
  begin
    NormalizedResponse := Lowercase(ResponseText);
    Result := (Pos('"service":"allclient-access"', NormalizedResponse) > 0) or
      (Pos('"valid":true', NormalizedResponse) > 0) or
      (Pos('"valid":false', NormalizedResponse) > 0);
  end;
end;

function OnlineAccessCodeIsValid(const EnteredCode: String): Boolean;
var
  RequestUrl: String;
  ResponseText: String;
begin
  Result := False;
  OnlineServiceUnavailable := False;
  OnlineVerificationMessage := '';

  if not IsEightDigitCode(EnteredCode) then
  begin
    OnlineVerificationMessage := 'Online codes must contain exactly 8 digits.';
    Exit;
  end;

  SetAccessStatus('Checking the entered code securely...', clGray);
  RequestUrl := '?action=verify&code=' + Trim(EnteredCode);
  if FetchAccessApi(RequestUrl, ResponseText) then
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
  ResponseText: String;
begin
  RefreshAccessButton.Enabled := False;
  OnlineVerificationMessage := '';
  SetAccessStatus('Preparing secure verification component...', clGray);
  try
    if FetchAccessApi('?action=status', ResponseText) and
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
  ResponseText: String;
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
    PreparationProgress.Position := 55;
    PreparationStatusLabel.Caption :=
      'Secure transports prepared. Confirming online access...';
    WizardForm.Update;

    if FetchAccessApi('?action=status', ResponseText) and
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
        'Secure transports are ready, but the online service could not be confirmed. You may retry or continue with offline access.';
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

function ReadPreviousInstallFromRoot(RootKey: Integer;
  var InstallDirectory, InstalledVersion: String): Boolean;
begin
  Result := False;
  InstallDirectory := '';
  InstalledVersion := '';

  if not RegQueryStringValue(RootKey, AllclientUninstallKey,
    'InstallLocation', InstallDirectory) then
    Exit;

  InstallDirectory := RemoveBackslashUnlessRoot(Trim(InstallDirectory));
  RegQueryStringValue(RootKey, AllclientUninstallKey,
    'DisplayVersion', InstalledVersion);
  Result := InstallDirectory <> '';
end;

function FindPreviousAllclient(var InstallDirectory,
  InstalledVersion: String): Boolean;
begin
  Result := ReadPreviousInstallFromRoot(HKCU, InstallDirectory,
    InstalledVersion);
  if not Result and IsWin64 then
    Result := ReadPreviousInstallFromRoot(HKLM64, InstallDirectory,
      InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromRoot(HKLM32, InstallDirectory,
      InstalledVersion);
end;

function PreviousInstallPathIsSafe(const InstallDirectory: String): Boolean;
var
  NormalInstallDirectory: String;
begin
  NormalInstallDirectory := RemoveBackslashUnlessRoot(
    ExpandFileName(InstallDirectory));

  Result := (Length(NormalInstallDirectory) > 3) and
    (CompareText(NormalInstallDirectory,
      RemoveBackslashUnlessRoot(ExpandConstant('{win}'))) <> 0) and
    (CompareText(NormalInstallDirectory,
      RemoveBackslashUnlessRoot(ExpandConstant('{sys}'))) <> 0) and
    (CompareText(NormalInstallDirectory,
      RemoveBackslashUnlessRoot(ExpandConstant('{tmp}'))) <> 0) and
    (CompareText(NormalInstallDirectory,
      RemoveBackslashUnlessRoot(ExpandConstant('{localappdata}'))) <> 0) and
    (CompareText(NormalInstallDirectory,
      RemoveBackslashUnlessRoot(ExpandConstant('{userappdata}'))) <> 0);
end;

procedure RemovePreviousRegistrationAndShortcuts;
begin
  RegDeleteKeyIncludingSubkeys(HKCU, AllclientUninstallKey);
  if IsWin64 then
    RegDeleteKeyIncludingSubkeys(HKLM64, AllclientUninstallKey);
  RegDeleteKeyIncludingSubkeys(HKLM32, AllclientUninstallKey);

  DeleteFile(ExpandConstant('{autodesktop}\Allclient - Voice Enabled.lnk'));
  DeleteFile(ExpandConstant('{autodesktop}\Allclient - No Voice.lnk'));
  DelTree(ExpandConstant('{group}'), True, True, True);
end;

function RemovePreviousAllclient(var ErrorMessage: String): Boolean;
var
  InstallDirectory, InstalledVersion: String;
begin
  Result := False;
  ErrorMessage := '';

  { If a locked file prevented direct cleanup, retry only the directory that
    was previously read from Allclient's own registered installation record. }
  if PreviousInstallDirectoryPendingCleanup <> '' then
  begin
    if (not DirExists(PreviousInstallDirectoryPendingCleanup)) or
       DelTree(PreviousInstallDirectoryPendingCleanup, True, True, True) then
    begin
      RemovePreviousRegistrationAndShortcuts;
      PreviousInstallDirectoryPendingCleanup := '';
      Result := True;
    end
    else
      ErrorMessage :=
        'Some files from the previous Allclient installation are still in use. Close the game and launcher, then try again.';
    Exit;
  end;

  if not FindPreviousAllclient(InstallDirectory, InstalledVersion) then
  begin
    Result := True;
    Exit;
  end;

  if not PreviousInstallPathIsSafe(InstallDirectory) then
  begin
    ErrorMessage :=
      'A previous Allclient installation was detected, but its registered directory is unsafe to remove automatically.';
    Exit;
  end;

  SetAccessStatus('Cleaning the previous Allclient installation...', clGray);
  PreviousInstallDirectoryPendingCleanup := InstallDirectory;
  if DirExists(InstallDirectory) and
     not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'Some files from the previous Allclient installation are still in use. Close the game and launcher, then try again.';
    Exit;
  end;

  RemovePreviousRegistrationAndShortcuts;
  PreviousInstallDirectoryPendingCleanup := '';
  Result := True;
end;

function DirectoryHasEntries(const Directory: String): Boolean;
var
  FindRec: TFindRec;
begin
  Result := False;
  if FindFirst(AddBackslash(Directory) + '*', FindRec) then
  begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          Result := True;
          Exit;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

function DirectoryLooksLikeAllclient(const Directory: String): Boolean;
begin
  Result :=
    FileExists(AddBackslash(Directory) + 'cstrike.exe') or
    FileExists(AddBackslash(Directory) + 'unins000.exe') or
    DirExists(AddBackslash(Directory) + 'platform\steam\games\SmartEmu') or
    DirExists(AddBackslash(Directory) + 'platform\steam\games\SmartEmu2');
end;

function CleanSelectedInstallDirectory(var ErrorMessage: String): Boolean;
var
  InstallDirectory, BuildSourceDirectory: String;
begin
  Result := False;
  ErrorMessage := '';
  InstallDirectory := RemoveBackslashUnlessRoot(
    ExpandFileName(ExpandConstant('{app}')));
  BuildSourceDirectory := RemoveBackslashUnlessRoot(
    ExpandFileName('{#SourceRoot}'));

  if not PreviousInstallPathIsSafe(InstallDirectory) then
  begin
    ErrorMessage :=
      'The selected installation directory is unsafe to clean automatically.';
    Exit;
  end;

  { Protect the developer/source payload if Setup is tested on the build PC. }
  if CompareText(InstallDirectory, BuildSourceDirectory) = 0 then
  begin
    ErrorMessage :=
      'The selected directory is the installer source directory and cannot be cleaned.';
    Exit;
  end;

  if not DirExists(InstallDirectory) then
  begin
    Result := True;
    Exit;
  end;

  if DirectoryHasEntries(InstallDirectory) and
     not DirectoryLooksLikeAllclient(InstallDirectory) then
  begin
    ErrorMessage :=
      'The selected directory contains unrelated files and was not removed. Choose an empty directory or the existing Allclient directory.';
    Exit;
  end;

  SetAccessStatus('Cleaning the selected Allclient directory...', clGray);
  if not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'The existing Allclient directory could not be removed completely. Close the game and launcher, then try again.';
    Exit;
  end;

  Result := True;
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
      begin
        AccessApproved := True;
        SetAccessStatus(OnlineVerificationMessage, clGreen)
      end
      else
      begin
        AccessApproved := False;
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

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  if not AccessApproved then
  begin
    Result := 'Installation access must be verified before setup can continue.';
    Exit;
  end;

  if PreviousInstallCleanupDone then
  begin
    if not DestinationCleanupDone and
       CleanSelectedInstallDirectory(Result) then
      DestinationCleanupDone := True;
    Exit;
  end;

  if RemovePreviousAllclient(Result) then
  begin
    PreviousInstallCleanupDone := True;
    if CleanSelectedInstallDirectory(Result) then
      DestinationCleanupDone := True;
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

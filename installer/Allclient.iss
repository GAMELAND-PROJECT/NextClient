#define AppName "Allclient"
#define AppVersion "2.5.3"
#define AppPublisher "GAMELAND PROJECT"
#define AppExeName "cstrike.exe"
#define TagFile FileOpen("..\client_tags.txt")
#define BuildTag Trim(FileRead(TagFile))
#expr FileClose(TagFile)

#ifndef SourceRoot
  #define SourceRoot "F:\CS 1.6 - AllClient"
#endif

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

[Languages]
Name: "farsi"; MessagesFile: "languages\Farsi.isl"

[Files]
Source: "runtime\vc_redist.x86.exe"; Flags: dontcopy
Source: "runtime\vc_redist.x64.exe"; Flags: dontcopy
Source: "runtime\vcredist2010_x86.exe"; Flags: dontcopy
Source: "runtime\vcredist2010_x64.exe"; Flags: dontcopy
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "crashes\*,htmlcache\*,*.log,*.mdmp,debug.log,install.bat,unins000.exe,unins000.dat"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}_is1"; ValueType: string; ValueName: "GameNetTag"; ValueData: "{#BuildTag}"; Flags: uninsdeletevalue

[Icons]
Name: "{autodesktop}\Allclient - Voice Enabled"; Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; IconFilename: "{app}\cstrike.exe"
Name: "{autodesktop}\Allclient - No Voice"; Filename: "{app}\platform\steam\games\SmartEmu2\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu2"; IconFilename: "{app}\cstrike.exe"
Name: "{group}\حذف Allclient"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\platform\steam\games\SmartEmu\SSELauncher.exe"; Parameters: "-appid 10"; WorkingDir: "{app}\platform\steam\games\SmartEmu"; Description: "اجرای Allclient با گفت‌وگوی صوتی"; Flags: nowait postinstall skipifsilent unchecked

[Code]
const
  { The target Windows 7 systems can reach this first-party endpoint over
    HTTP, while their obsolete TLS/certificate stacks reject its HTTPS route. }
  AccessApiUrl = 'http://gameland.cam/installer_access.php';
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
  OnlineVerificationMessage: String;
  AccessApproved: Boolean;
  PreviousInstallCleanupDone: Boolean;
  PreviousInstallDirectoryPendingCleanup: String;
  DestinationCleanupDone: Boolean;
  UpdateAccessChecked: Boolean;
  DependenciesReady: Boolean;

function URLDownloadToFile(Caller: NativeInt; URL, FileName: String;
  Reserved: DWORD; StatusCallback: NativeInt): HResult;
  external 'URLDownloadToFileW@urlmon.dll stdcall delayload';

procedure SetAccessStatus(const Caption: String; Color: TColor);
begin
  AccessStatusLabel.Font.Color := Color;
  AccessStatusLabel.Caption := Caption;
  WizardForm.Update;
end;

function NormalizeAccessCode(const Value: String): String;
var
  I, Digit: Integer;
begin
  Result := Trim(Value);
  for I := 1 to Length(Result) do
  begin
    Digit := Ord(Result[I]);
    if (Digit >= $06F0) and (Digit <= $06F9) then
      Result[I] := Chr(Ord('0') + Digit - $06F0)
    else if (Digit >= $0660) and (Digit <= $0669) then
      Result[I] := Chr(Ord('0') + Digit - $0660);
  end;
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

function FetchWithNativeRequest(const Url: String; var ResponseText: String): Boolean;
var
  Request: Variant;
  ProxyMode: Integer;
begin
  Result := False;
  ResponseText := '';
  if (Pos('https://', Lowercase(Url)) <> 1) and
     (Pos('http://', Lowercase(Url)) <> 1) then
  begin
    OnlineVerificationMessage := 'نشانی سرویس تأیید معتبر نیست.';
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
      if Pos('https://', Lowercase(Url)) = 1 then
        Request.Option[9] := 2048; { TLS 1.2 only for the secure route. }
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
      { Try the direct transport and then retry through the native stack. }
      ResponseText := '';
    end;
  end;
end;

function FetchWithInternetExplorer(const Url: String; var ResponseText: String): Boolean;
var
  DownloadPath: String;
  RawResponse: AnsiString;
  ResponseSize: Integer;
begin
  Result := False;
  ResponseText := '';
  DownloadPath := ExpandConstant('{tmp}\allclient-ie-response.tmp');

  try
    try
      DeleteFile(DownloadPath);
      { URLMon uses the same WinINet, certificate store and proxy configuration
        as Internet Explorer. This is the preferred Windows 7 transport. }
      if URLDownloadToFile(0, Url, DownloadPath, 0, 0) <> 0 then
        Exit;
      if not FileSize(DownloadPath, ResponseSize) or
         (ResponseSize <= 0) or (ResponseSize > 262144) then
        Exit;
      if not LoadStringFromFile(DownloadPath, RawResponse) then
        Exit;

      ResponseText := RawResponse;
      Result := Length(ResponseText) > 0;
    except
      ResponseText := '';
      Result := False;
    end;
  finally
    DeleteFile(DownloadPath);
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

function FetchAccessResponse(const Url: String; var ResponseText: String): Boolean;
var
  Attempt: Integer;
begin
  Result := False;
  ResponseText := '';
  for Attempt := 1 to 2 do
  begin
    SetAccessStatus('در حال اتصال به سرویس؛ تلاش ' +
      IntToStr(Attempt) + ' از ۲…', clGray);
    try
      if FetchWithInternetExplorer(Url, ResponseText) then
      begin
        Result := True;
        Exit;
      end
      else
      begin
        SetAccessStatus('اتصال برقرار نشد؛ در حال بررسی روش جایگزین…', clGray);
        if FetchWithSetupDownloader(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;

        SetAccessStatus('در حال تلاش با روش سازگار با ویندوز…', clGray);
        if FetchWithNativeRequest(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;

      end;

      if Attempt < 2 then
        SetAccessStatus('ارتباط قطع شد؛ در حال تلاش دوباره…', clGray);
    except
      OnlineVerificationMessage := 'پاسخ سرویس قابل پردازش نیست.';
      Result := False;
    end;
  end;
end;

function FetchAccessApi(const Query: String; var ResponseText: String): Boolean;
var
  NormalizedResponse: String;
begin
  SetAccessStatus('در حال اتصال به سرویس تأیید Allclient…', clGray);
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
    OnlineVerificationMessage := 'کد آنلاین باید دقیقاً ۸ رقم باشد.';
    Exit;
  end;

  SetAccessStatus('در حال بررسی کد واردشده…', clGray);
  RequestUrl := '?action=verify&code=' + Trim(EnteredCode);
  if FetchAccessApi(RequestUrl, ResponseText) then
  begin
    SetAccessStatus('پاسخ دریافت شد؛ در حال بررسی نتیجه…', clGray);
    if Pos('"valid":true', Lowercase(ResponseText)) > 0 then
    begin
      Result := True;
      OnlineVerificationMessage := 'کد آنلاین تأیید شد.';
    end
    else if Pos('"valid":false', Lowercase(ResponseText)) > 0 then
      OnlineVerificationMessage := 'کد آنلاین اشتباه است یا لغو شده است.'
    else
    begin
      OnlineServiceUnavailable := True;
      OnlineVerificationMessage := 'پاسخ سرویس آنلاین معتبر نیست.';
    end;
  end
  else
  begin
    OnlineServiceUnavailable := True;
    if OnlineVerificationMessage = '' then
      OnlineVerificationMessage := 'سرویس تأیید آنلاین در دسترس نیست.';
  end;
end;

function AccessCodeIsValid(const EnteredCode: String): Boolean;
begin
  Result := CompareText(Trim(EnteredCode), OfflineCode) = 0;
  if Result then
    OnlineVerificationMessage := 'کد دسترسی تأیید شد.'
  else
    Result := OnlineAccessCodeIsValid(EnteredCode);
end;

procedure RefreshAccessStatus(Sender: TObject);
var
  ResponseText: String;
begin
  RefreshAccessButton.Enabled := False;
  OnlineVerificationMessage := '';
  SetAccessStatus('در حال آماده‌سازی بررسی اتصال…', clGray);
  try
    if FetchAccessApi('?action=status', ResponseText) and
       (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
      SetAccessStatus('سرویس آنلاین متصل است و پاسخ می‌دهد.', clGreen)
    else
    begin
      if OnlineVerificationMessage = '' then
        OnlineVerificationMessage := 'وضعیت معتبری از سرویس دریافت نشد.';
      SetAccessStatus('سرویس در دسترس نیست: ' + OnlineVerificationMessage, clRed);
    end;
  except
    SetAccessStatus('بررسی اتصال با خطا مواجه شد؛ دوباره تلاش کنید.', clRed);
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
    'در حال آماده‌سازی اتصال برای تأیید آنلاین…';
  WizardForm.Update;

  try
    PreparationProgress.Position := 55;
    PreparationStatusLabel.Caption :=
      'اتصال آماده است؛ در حال بررسی دسترسی آنلاین…';
    WizardForm.Update;

    if FetchAccessApi('?action=status', ResponseText) and
       (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
    begin
      PreparationProgress.Position := 100;
      PreparationReady := True;
      PreparationStatusLabel.Font.Color := clGreen;
      PreparationStatusLabel.Caption :=
        'آماده‌سازی کامل شد؛ تأیید آنلاین آماده است.';
    end
    else
    begin
      PreparationProgress.Position := 55;
      PreparationStatusLabel.Font.Color := clRed;
      PreparationStatusLabel.Caption :=
        'سرویس آنلاین پاسخ نداد؛ دوباره تلاش کنید یا با کد آفلاین ادامه دهید.';
      PreparationRetryButton.Visible := True;
    end;
  except
    PreparationProgress.Position := 0;
    PreparationStatusLabel.Font.Color := clRed;
    PreparationStatusLabel.Caption :=
      'آماده‌سازی انجام نشد؛ دوباره تلاش کنید یا با کد آفلاین ادامه دهید.';
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
    'آماده‌سازی Allclient',
    'آماده‌سازی اولیه اتصال');

  PreparationStatusLabel := TNewStaticText.Create(WizardForm);
  PreparationStatusLabel.Parent := PreparationPage.Surface;
  PreparationStatusLabel.Left := ScaleX(12);
  PreparationStatusLabel.Top := ScaleY(34);
  PreparationStatusLabel.Width := PreparationPage.SurfaceWidth - ScaleX(24);
  PreparationStatusLabel.Height := ScaleY(54);
  PreparationStatusLabel.AutoSize := False;
  PreparationStatusLabel.WordWrap := True;
  PreparationStatusLabel.Caption :=
    'برای بررسی اتصال آنلاین آماده است.';
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
  PreparationRetryButton.Caption := 'تلاش دوباره';
  PreparationRetryButton.OnClick := @RunEarlyPreparation;
  PreparationRetryButton.Visible := False;

  AccessPage := CreateInputQueryPage(
    PreparationPage.ID,
    'تأیید مجوز نصب',
    'کد نصب Allclient را وارد کنید',
    'کد آنلاین ۸ رقمی فعال یا کد آفلاین را وارد کنید.');
  AccessPage.Add('کد دسترسی:', True);

  AccessStatusLabel := TNewStaticText.Create(WizardForm);
  AccessStatusLabel.Parent := AccessPage.Surface;
  AccessStatusLabel.Left := AccessPage.Edits[0].Left;
  AccessStatusLabel.Top := AccessPage.Edits[0].Top + AccessPage.Edits[0].Height + ScaleY(20);
  AccessStatusLabel.Width := AccessPage.SurfaceWidth;
  AccessStatusLabel.Height := ScaleY(42);
  AccessStatusLabel.AutoSize := False;
  AccessStatusLabel.WordWrap := True;
  AccessStatusLabel.Caption := 'اتصال آنلاین هنوز بررسی نشده است.';
  AccessStatusLabel.Font.Color := clGray;

  RefreshAccessButton := TNewButton.Create(WizardForm);
  RefreshAccessButton.Parent := AccessPage.Surface;
  RefreshAccessButton.Left := AccessPage.Edits[0].Left;
  RefreshAccessButton.Top := AccessStatusLabel.Top + AccessStatusLabel.Height + ScaleY(10);
  RefreshAccessButton.Width := ScaleX(150);
  RefreshAccessButton.Height := ScaleY(30);
  RefreshAccessButton.Caption := 'بررسی دوباره اتصال';
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
        'بعضی فایل‌های نسخه قبلی در حال استفاده‌اند. بازی و لانچر را ببندید و دوباره تلاش کنید.';
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
      'نسخه قبلی پیدا شد، اما پوشه ثبت‌شده آن برای حذف خودکار مناسب نیست.';
    Exit;
  end;

  SetAccessStatus('در حال پاک‌سازی نسخه قبلی Allclient…', clGray);
  PreviousInstallDirectoryPendingCleanup := InstallDirectory;
  if DirExists(InstallDirectory) and
     not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'بعضی فایل‌های نسخه قبلی در حال استفاده‌اند. بازی و لانچر را ببندید و دوباره تلاش کنید.';
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
      'پوشه انتخاب‌شده برای پاک‌سازی خودکار مناسب نیست.';
    Exit;
  end;

  { Protect the developer/source payload if Setup is tested on the build PC. }
  if CompareText(InstallDirectory, BuildSourceDirectory) = 0 then
  begin
    ErrorMessage :=
      'پوشه انتخاب‌شده منبع ساخت نصب‌کننده است و نمی‌توان آن را پاک کرد.';
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
      'پوشه شامل فایل‌های نامرتبط است و حذف نشد. پوشه خالی یا پوشه نسخه قبلی Allclient را انتخاب کنید.';
    Exit;
  end;

  SetAccessStatus('در حال پاک‌سازی پوشه انتخاب‌شده Allclient…', clGray);
  if not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'حذف کامل پوشه قبلی ممکن نشد. بازی و لانچر را ببندید و دوباره تلاش کنید.';
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
    SetAccessStatus('در حال تأیید مجوز نصب…', clGray);
    try
      try
        Result := AccessCodeIsValid(NormalizeAccessCode(AccessPage.Values[0]));
      except
        Result := False;
        OnlineServiceUnavailable := True;
        OnlineVerificationMessage := 'تأیید با خطای غیرمنتظره متوقف شد؛ دوباره تلاش کنید.';
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
          OnlineVerificationMessage := 'کد نصب تأیید نشد.';
        SetAccessStatus(OnlineVerificationMessage, clRed);
        MsgBox(OnlineVerificationMessage, mbError, MB_OK);
        WizardForm.ActiveControl := AccessPage.Edits[0];
      end;
    finally
      WizardForm.NextButton.Enabled := True;
    end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
var
  Directory, Version, InstalledTag, Response: String;
begin
  Result := False;
  if PageID <> AccessPage.ID then Exit;
  if not UpdateAccessChecked then
  begin
    UpdateAccessChecked := True;
    if FindPreviousAllclient(Directory, Version) and
       FileExists(AddBackslash(Directory) + 'cstrike.exe') and
       RegQueryStringValue(HKCU, AllclientUninstallKey, 'GameNetTag', InstalledTag) and
       (CompareText(InstalledTag, '{#BuildTag}') = 0) then
    begin
      if FetchAccessResponse('http://gameland.cam/update_access.php?tag={#BuildTag}', Response) then
        AccessApproved := Trim(Response) = 'ACTIVE|{#BuildTag}';
    end;
  end;
  Result := AccessApproved;
end;

function InstallRuntime(const FileName, Parameters: String;
  var NeedsRestart: Boolean): Boolean;
var
  Code: Integer;
begin
  ExtractTemporaryFile(FileName);
  Result := ShellExec('runas', ExpandConstant('{tmp}\') + FileName,
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, Code);
  if not Result then Exit;
  if (Code = 3010) or (Code = 1641) then NeedsRestart := True;
  { 1638 means this runtime family already has a newer installed version. }
  Result := (Code = 0) or (Code = 3010) or (Code = 1641) or (Code = 1638);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  if not AccessApproved then
  begin
    Result := 'برای ادامه، ابتدا کد نصب را تأیید کنید.';
    Exit;
  end;

  if not DependenciesReady then
  begin
    SetAccessStatus('در حال نصب پیش‌نیازها؛ درخواست دسترسی مدیر ویندوز را تأیید کنید.', clGray);
    DependenciesReady := InstallRuntime('vc_redist.x86.exe', '/install /quiet /norestart', NeedsRestart);
    if DependenciesReady then
      DependenciesReady := InstallRuntime('vcredist2010_x86.exe', '/q /norestart', NeedsRestart);
    if DependenciesReady and IsWin64 then
      DependenciesReady := InstallRuntime('vc_redist.x64.exe', '/install /quiet /norestart', NeedsRestart);
    if DependenciesReady and IsWin64 then
      DependenciesReady := InstallRuntime('vcredist2010_x64.exe', '/q /norestart', NeedsRestart);
    if not DependenciesReady then
    begin
      Result := 'نصب پیش‌نیازها کامل نشد. دسترسی مدیر را تأیید کنید و دوباره تلاش کنید. نسخه قبلی حذف نشده است.';
      Exit;
    end;
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
    RaiseException('خواندن فایل تنظیمات ممکن نشد: ' + FileName);

  OpenTag := '<' + ElementName + '>';
  CloseTag := '</' + ElementName + '>';
  ValueStart := Pos(OpenTag, Content);
  if ValueStart = 0 then
    RaiseException('بخش تنظیمات پیدا نشد: ' + ElementName + ' در ' + FileName);

  ValueStart := ValueStart + Length(OpenTag);
  Tail := Copy(Content, ValueStart, MaxInt);
  CloseRelative := Pos(CloseTag, Tail);
  if CloseRelative = 0 then
    RaiseException('بخش تنظیمات نامعتبر است: ' + ElementName + ' در ' + FileName);

  Delete(Content, ValueStart, CloseRelative - 1);
  Insert(NewValue, Content, ValueStart);
  if not SaveStringToFile(FileName, Content, False) then
    RaiseException('ذخیره فایل تنظیمات ممکن نشد: ' + FileName);
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

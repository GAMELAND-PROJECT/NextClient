#define AppName "Allclient"
#ifndef AppVersion
  #define AppVersion "0.0.1"
#endif
#define AppPublisher "GAMELAND PROJECT"
#define AppExeName "cstrike.exe"
#ifndef BuildTag
  #define TagFile FileOpen("..\client_tags.txt")
  #define BuildTag Trim(FileRead(TagFile))
  #expr FileClose(TagFile)
#endif

#ifndef SourceRoot
  #define SourceRoot "F:\Allclient"
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
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=Allclient-Setup
DiskSpanning=yes
DiskSliceSize=Max
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

#ifndef BinaryRoot
  #define BinaryRoot "..\install"
#endif

[Files]
Source: "runtime\vc_redist.x86.exe"; Flags: dontcopy
Source: "runtime\vc_redist.x64.exe"; Flags: dontcopy
Source: "runtime\vcredist2010_x86.exe"; Flags: dontcopy
Source: "runtime\vcredist2010_x64.exe"; Flags: dontcopy
; 1. Base files excluding maps and user config (so custom maps are never overwritten)
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "cstrike\maps\*,cstrike\userconfig.cfg,backups\*,cstrike_downloads\*,crashes\*,htmlcache\*,*.log,*.mdmp,debug.log,install.bat,unins000.exe,unins000.dat,*.bak*,hitbox_vis.asi*,*.asi.disabled,auto_launcher_tests.exe,allclient-install.ini"
; 2. Game maps - NEVER overwrite existing maps! Custom and downloaded maps are 100% preserved
Source: "{#SourceRoot}\cstrike\maps\*"; DestDir: "{app}\cstrike\maps"; Flags: onlyifdoesntexist recursesubdirs createallsubdirs; Excludes: "*.log,*.bak*"
; 3. User config template - only install if not already existing
Source: "{#SourceRoot}\cstrike\userconfig.cfg"; DestDir: "{app}\cstrike"; Flags: onlyifdoesntexist;
; 4. Overlay latest compiled binaries and configs
Source: "{#BinaryRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "cstrike\maps\*,*.log,*.mdmp,debug.log,hitbox_vis.asi*,*.asi.disabled,auto_launcher_tests.exe,allclient-install.ini"

[INI]
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "Schema"; String: "1"
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "ClientType"; String: "Home"
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "GameNetTag"; String: ""
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "DeviceHash"; String: "{code:GetDeviceHash}"
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "PhoneNumber"; String: "{code:GetUserPhoneNumber}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}_is1"; ValueType: string; ValueName: "GameNetTag"; ValueData: "{code:GetActiveGameNetTag}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\NextClient"; ValueType: string; ValueName: "InstallID"; ValueData: "{code:GetHardwareID}"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\NextClient"; ValueType: string; ValueName: "InstallID"; ValueData: "{code:GetHardwareID}"; Flags: uninsdeletevalue noerror

[Icons]
Name: "{autodesktop}\Allclient"; Filename: "{app}\Allclient.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Allclient.exe"
Name: "{group}\حذف Allclient"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Allclient.exe"; WorkingDir: "{app}"; Description: "اجرای Allclient"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function InitializeSetup(): Boolean;
var
  SetupDir, BinSlice: String;
  BinSize: Integer;
begin
  Result := True;
  SetupDir := ExtractFilePath(ExpandConstant('{srcexe}'));
  BinSlice := SetupDir + 'Allclient-Setup-1.bin';

  { Anti-tamper & Data integrity check for 2-piece setup }
  if not FileExists(BinSlice) then
  begin
    MsgBox('خطای امنیتی: فایل داده‌های بازی (Allclient-Setup-1.bin) در کنار برنامه نصب یافت نشد.' + #13#10#13#10 +
           'لطفاً هر دو فایل Allclient-Setup.exe و Allclient-Setup-1.bin را در یک پوشه قرار دهید.', mbCriticalError, MB_OK);
    Result := False;
    Exit;
  end;

  if not FileSize(BinSlice, BinSize) or (BinSize < 100000000) then
  begin
    MsgBox('خطای امنیتی: فایل داده‌های بازی (Allclient-Setup-1.bin) ناقص یا دستکاری شده است.' + #13#10#13#10 +
           'حجم فایل معتبر نیست. لطفاً مجدداً فایل کامل را دریافت فرمایید.', mbCriticalError, MB_OK);
    Result := False;
    Exit;
  end;
end;

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
external 'GetVolumeInformationW@kernel32.dll stdcall';

function GetHardwareID(Param: String): String;
var
  SerialNum, MaxLen, Flags: DWORD;
begin
  if GetVolumeInformation('C:\', '', 0, SerialNum, MaxLen, Flags, '', 0) then
    Result := Format('%.8X', [SerialNum])
  else
    Result := 'UNKNOWN_HWID';
end;

const
  { The target Windows 7 systems can reach this first-party endpoint over
    HTTP, while their obsolete TLS/certificate stacks reject its HTTPS route. }
  AccessApiUrl = 'http://gameland.cam/installer_access.php';
  OfflineCode = 'amir1394';
  AllclientUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}_is1';

var
  HomeActivationPage: TWizardPage;
  HomeDescLabel: TNewStaticText;
  HomeHashLabel: TNewStaticText;
  HomeHashEdit: TNewEdit;
  HomeCopyButton: TNewButton;
  HomePhoneLabel: TNewStaticText;
  HomePhoneEdit: TNewEdit;
  HomeStatusLabel: TNewStaticText;
  HomeVerifyButton: TNewButton;
  DeviceHash24: String;
  UserPhoneNumber: String;
  OnlineServiceUnavailable: Boolean;
  OnlineVerificationMessage: String;
  AccessApproved: Boolean;
  PreviousInstallCleanupDone: Boolean;
  PreviousInstallDirectoryPendingCleanup: String;
  DestinationCleanupDone: Boolean;
  UpdateAccessChecked: Boolean;
  SubscriptionUpdate: Boolean;
  DetectedInstallDirectory: String;
  DetectedInstallRoot: Integer;
  DependenciesReady: Boolean;
  ActiveGameNetTag: String;
  IsPatchMode: Boolean;

function GetDeviceHash(Param: String): String;
begin
  Result := DeviceHash24;
end;

function GetUserPhoneNumber(Param: String): String;
begin
  Result := Trim(UserPhoneNumber);
end;

function Compute24CharDeviceHash(): String;
var
  MachineGuid: String;
  VolSerial, MaxLen, Flags: DWORD;
  ComputerName: String;
  CpuName: String;
  RawSeed: String;
  Sha256Hex: String;
begin
  MachineGuid := '';
  if IsWin64 then
    RegQueryStringValue(HKLM64, 'SOFTWARE\Microsoft\Cryptography', 'MachineGuid', MachineGuid);
  if MachineGuid = '' then
    RegQueryStringValue(HKLM, 'SOFTWARE\Microsoft\Cryptography', 'MachineGuid', MachineGuid);
  if MachineGuid = '' then
    RegQueryStringValue(HKLM32, 'SOFTWARE\Microsoft\Cryptography', 'MachineGuid', MachineGuid);

  VolSerial := 0;
  MaxLen := 0;
  Flags := 0;
  GetVolumeInformation('C:\', '', 0, VolSerial, MaxLen, Flags, '', 0);

  ComputerName := ExpandConstant('{computername}');

  CpuName := '';
  RegQueryStringValue(HKLM, 'HARDWARE\DESCRIPTION\System\CentralProcessor\0', 'ProcessorNameString', CpuName);

  RawSeed := MachineGuid + '|' + Format('%.8X', [VolSerial]) + '|' + ComputerName + '|' + CpuName;
  if Trim(RawSeed) = '|||' then
    RawSeed := Format('FALLBACK_%.8X', [VolSerial]);

  Sha256Hex := Uppercase(GetSHA256OfString(RawSeed));
  Result := Copy(Sha256Hex, 1, 24);
end;

procedure CopyDeviceHashClick(Sender: TObject);
begin
  HomeHashEdit.SelectAll;
  SendMessage(HomeHashEdit.Handle, 769 {WM_COPY}, 0, 0);
  HomeCopyButton.Caption := '✓ کپی شد!';
end;

function FetchAccessApi(const Query: String; var ResponseText: String): Boolean; forward;
function GetJsonString(const Json, Key: String): String; forward;

function NormalizeDigitsAndPhone(const S: String): String;
var
  I: Integer;
  C: Char;
  Res: String;
begin
  Res := '';
  for I := 1 to Length(S) do
  begin
    C := S[I];
    if (Ord(C) >= $06F0) and (Ord(C) <= $06F9) then
      Res := Res + Chr(Ord(C) - $06F0 + Ord('0'))
    else if (Ord(C) >= $0660) and (Ord(C) <= $0669) then
      Res := Res + Chr(Ord(C) - $0660 + Ord('0'))
    else if (C >= '0') and (C <= '9') then
      Res := Res + C
    else if (C = '+') and (Res = '') then
      Res := '+';
  end;
  if (Length(Res) >= 3) and (Copy(Res, 1, 3) = '+98') then
    Res := '0' + Copy(Res, 4, Length(Res) - 3)
  else if (Length(Res) >= 2) and (Copy(Res, 1, 2) = '98') and (Length(Res) = 12) then
    Res := '0' + Copy(Res, 3, Length(Res) - 2);

  Result := Trim(Res);
end;

function HomeAccessVerify(const Hash, Phone: String): Boolean;
var
  CleanHash: String;
  CleanPhone: String;
  RequestUrl: String;
  ResponseText: String;
  NormalizedResponse: String;
  DaysRemainingStr: String;
  ExpiryStr: String;
  ServerErr: String;
begin
  Result := False;
  OnlineServiceUnavailable := False;
  OnlineVerificationMessage := '';

  CleanHash := Uppercase(Trim(Hash));
  CleanPhone := NormalizeDigitsAndPhone(Phone);

  if Length(CleanHash) <> 24 then
  begin
    OnlineVerificationMessage := 'کد سخت‌افزاری دستگاه باید دقیقاً ۲۴ کاراکتر باشد.';
    Exit;
  end;

  if Length(CleanPhone) = 0 then
  begin
    OnlineVerificationMessage := 'لطفاً شماره تلفن همراه ثبت‌شده در پنل را وارد نمایید.';
    Exit;
  end;

  HomePhoneEdit.Text := CleanPhone;
  HomeStatusLabel.Caption := 'در حال بررسی اتصال و وضعیت اشتراک…';
  HomeStatusLabel.Font.Color := clGray;
  WizardForm.Update;

  RequestUrl := '?action=verify_home&hash=' + CleanHash + '&phone=' + CleanPhone;
  if FetchAccessApi(RequestUrl, ResponseText) then
  begin
    NormalizedResponse := Lowercase(ResponseText);
    if Pos('"valid":true', NormalizedResponse) > 0 then
    begin
      Result := True;
      UserPhoneNumber := CleanPhone;
      DaysRemainingStr := GetJsonString(ResponseText, 'days_remaining');
      ExpiryStr := GetJsonString(ResponseText, 'expiry');
      OnlineVerificationMessage := '✓ اشتراک کلاینت خانگی با موفقیت تأیید شد. اعتبار: ' + DaysRemainingStr + ' روز باقی‌مانده (تا تاریخ ' + ExpiryStr + ')';
    end
    else
    begin
      Result := False;
      ServerErr := GetJsonString(ResponseText, 'error');
      if ServerErr <> '' then
        OnlineVerificationMessage := ServerErr
      else
        OnlineVerificationMessage := 'این کد دستگاه هنوز با این شماره تلفن در پنل مدیریت تأیید نشده است.';
    end;
  end
  else
  begin
    OnlineServiceUnavailable := True;
    OnlineVerificationMessage := 'خطا در برقراری ارتباط با سرور تأیید آنلاین. اتصال اینترنت را بررسی کنید.';
  end;
end;

procedure VerifyHomeClientClick(Sender: TObject);
begin
  HomeVerifyButton.Enabled := False;
  try
    if HomeAccessVerify(HomeHashEdit.Text, HomePhoneEdit.Text) then
    begin
      AccessApproved := True;
      HomeStatusLabel.Caption := OnlineVerificationMessage;
      HomeStatusLabel.Font.Color := clGreen;
      if (DetectedInstallDirectory <> '') and (CompareText(WizardForm.DirEdit.Text, DetectedInstallDirectory) = 0) then
        SubscriptionUpdate := True;
      MsgBox(OnlineVerificationMessage, mbInformation, MB_OK);
    end
    else
    begin
      AccessApproved := False;
      HomeStatusLabel.Caption := OnlineVerificationMessage;
      HomeStatusLabel.Font.Color := clRed;
      MsgBox(OnlineVerificationMessage, mbError, MB_OK);
    end;
  finally
    HomeVerifyButton.Enabled := True;
  end;
end;

function GetActiveGameNetTag(Param: String): String;
begin
  if Trim(ActiveGameNetTag) <> '' then
    Result := Trim(ActiveGameNetTag)
  else
    Result := '{#BuildTag}';
end;

function URLDownloadToFile(Caller: NativeInt; URL, FileName: String;
  Reserved: DWORD; StatusCallback: NativeInt): HResult;
  external 'URLDownloadToFileW@urlmon.dll stdcall delayload';

function SetFileAttributes(lpFileName: String; dwFileAttributes: DWORD): BOOL;
  external 'SetFileAttributesW@kernel32.dll stdcall';



procedure SetAccessStatus(const Caption: String; Color: TColor);
begin
  try
    if HomeStatusLabel <> nil then
    begin
      HomeStatusLabel.Font.Color := Color;
      HomeStatusLabel.Caption := Caption;
    end;
    WizardForm.Update;
  except
  end;
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

function GetJsonString(const Json, Key: String): String;
var
  KeyStr: String;
  StartPos, EndPos: Integer;
begin
  Result := '';
  KeyStr := '"' + Key + '":"';
  StartPos := Pos(KeyStr, Json);
  if StartPos > 0 then
  begin
    StartPos := StartPos + Length(KeyStr);
    EndPos := Pos('"', Copy(Json, StartPos, Length(Json)));
    if EndPos > 0 then
      Result := Copy(Json, StartPos, EndPos - 1);
  end;
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
  Result := (InstallDirectory <> '') and
    FileExists(AddBackslash(InstallDirectory) + 'cstrike.exe');
  if Result then DetectedInstallRoot := RootKey;
end;

function ReadPreviousInstallFromDirectory(const CandidateDirectory: String;
  var InstallDirectory, InstalledVersion: String): Boolean;
begin
  InstallDirectory := RemoveBackslashUnlessRoot(ExpandFileName(CandidateDirectory));
  InstalledVersion := '';
  Result := (InstallDirectory <> '') and
    FileExists(AddBackslash(InstallDirectory) + 'cstrike.exe');
  if Result then DetectedInstallRoot := HKCU;
end;

function FindPreviousAllclient(var InstallDirectory,
  InstalledVersion: String): Boolean;
var
  SteamPath, NextClientPath: String;
begin
  { 1. Check registered uninstall records }
  Result := ReadPreviousInstallFromRoot(HKCU, InstallDirectory,
    InstalledVersion);
  if not Result and IsWin64 then
    Result := ReadPreviousInstallFromRoot(HKLM64, InstallDirectory,
      InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromRoot(HKLM32, InstallDirectory,
      InstalledVersion);

  { 2. Check NextClient registry }
  if not Result and RegQueryStringValue(HKCU, 'Software\NextClient', 'GamePath', NextClientPath) then
    Result := ReadPreviousInstallFromDirectory(NextClientPath, InstallDirectory, InstalledVersion);

  { 3. Check AppData locations }
  if not Result then
    Result := ReadPreviousInstallFromDirectory(
      ExpandConstant('{localappdata}\Allclient'), InstallDirectory,
      InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory(
      ExpandConstant('{userappdata}\Allclient'), InstallDirectory,
      InstalledVersion);

  { 4. Deep scan common drives and custom installation paths }
  if not Result then
    Result := ReadPreviousInstallFromDirectory('D:\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory('C:\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory('E:\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory('F:\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory('D:\Games\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory('C:\Games\Allclient', InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory(ExpandConstant('{commonpf32}\Allclient'), InstallDirectory, InstalledVersion);
  if not Result then
    Result := ReadPreviousInstallFromDirectory(ExpandConstant('{commonpf32}\Counter-Strike'), InstallDirectory, InstalledVersion);

  { 5. Check Steam Half-Life / CS path if applicable }
  if not Result and RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
    Result := ReadPreviousInstallFromDirectory(SteamPath + '\steamapps\common\Half-Life', InstallDirectory, InstalledVersion);
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
  DeleteFile(ExpandConstant('{autodesktop}\Allclient.lnk'));
  DelTree(ExpandConstant('{group}'), True, True, True);
end;

function RemovePreviousAllclient(var ErrorMessage: String): Boolean;
var
  InstallDirectory, InstalledVersion: String;
  TargetAppDir: String;
begin
  Result := False;
  ErrorMessage := '';
  TargetAppDir := RemoveBackslashUnlessRoot(
    ExpandFileName(ExpandConstant('{app}')));

  { If a locked file prevented direct cleanup, retry only the directory that
    was previously read from Allclient's own registered installation record. }
  if PreviousInstallDirectoryPendingCleanup <> '' then
  begin
    if CompareText(TargetAppDir, PreviousInstallDirectoryPendingCleanup) = 0 then
    begin
      { User is installing/updating right inside the existing directory; preserve assets! }
      RemovePreviousRegistrationAndShortcuts;
      PreviousInstallDirectoryPendingCleanup := '';
      Result := True;
      Exit;
    end;

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

  { If installing into the exact same folder as the previously registered install,
    do NOT wipe the directory; let Smart Patch handle it safely without losing maps/configs. }
  if CompareText(TargetAppDir, InstallDirectory) = 0 then
  begin
    RemovePreviousRegistrationAndShortcuts;
    PreviousInstallDirectoryPendingCleanup := '';
    Result := True;
    Exit;
  end;

  { If user explicitly picked a completely DIFFERENT folder, clean up the old one so no orphan remains. }
  { Do NOT wipe external folders (e.g. D:\Allclient) even if user selected a different folder.
    Simply clean up old shortcuts and registry associations to protect user maps and data. }
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

procedure SmartPatchTargetDirectory(const TargetDir: String);
var
  BaseDir: String;
  MetamodIni: String;
  Lines: TArrayOfString;
  I: Integer;
begin
  BaseDir := AddBackslash(TargetDir);

  { 1. Remove outdated launcher and core executables }
  DeleteFile(BaseDir + 'Allclient.exe');
  DeleteFile(BaseDir + 'cstrike.exe');
  DeleteFile(BaseDir + 'hl.exe');
  DeleteFile(BaseDir + 'hltv.exe');
  DeleteFile(BaseDir + 'updater.exe');

  { 2. Remove outdated engine and proxy DLLs }
  DeleteFile(BaseDir + 'FileSystem_Proxy.dll');
  DeleteFile(BaseDir + 'next_engine_mini.dll');
  DeleteFile(BaseDir + 'nitro_api2.dll');
  DeleteFile(BaseDir + 'steam_api.dll');
  DeleteFile(BaseDir + 'vgui2.dll');

  { 3. Remove outdated client UI and hook DLLs }
  DeleteFile(BaseDir + 'cstrike\cl_dlls\client_mini.dll');
  DeleteFile(BaseDir + 'cstrike\cl_dlls\GameUI.dll');

  { 4. Clean up problematic/crash-inducing files and debug dumps }
  DeleteFile(BaseDir + 'hitbox_vis.asi');
  DeleteFile(BaseDir + 'hitbox_vis.asi.disabled');
  DeleteFile(BaseDir + 'cstrike\addons\hitbox_fixer\dlls\hitbox_fix_mm.dll');
  DeleteFile(BaseDir + 'debug.log');

  { 5. Ensure Metamod plugins.ini does not load buggy hitbox_fixer }
  MetamodIni := BaseDir + 'cstrike\addons\metamod\plugins.ini';
  if FileExists(MetamodIni) and LoadStringsFromFile(MetamodIni, Lines) then
  begin
    for I := 0 to GetArrayLength(Lines) - 1 do
    begin
      if (Pos('hitbox_fix_mm.dll', Lines[I]) > 0) and (Pos(';', Trim(Lines[I])) <> 1) then
        Lines[I] := '; ' + Lines[I];
    end;
    SaveStringsToFile(MetamodIni, Lines, False);
  end;
end;

function CleanSelectedInstallDirectory(var ErrorMessage: String): Boolean;
var
  InstallDirectory: String;
begin
  Result := False;
  ErrorMessage := '';
  InstallDirectory := RemoveBackslashUnlessRoot(
    ExpandFileName(ExpandConstant('{app}')));

  if not PreviousInstallPathIsSafe(InstallDirectory) then
  begin
    ErrorMessage :=
      'پوشه انتخاب‌شده برای پاک‌سازی خودکار مناسب نیست.';
    Exit;
  end;

  if not DirExists(InstallDirectory) then
  begin
    Result := True;
    Exit;
  end;

  { SMART PATCH MODE: If directory already has CS 1.6 / Allclient files,
    do NOT wipe the folder! Clean old binaries safely and overlay new ones. }
  if DirectoryLooksLikeAllclient(InstallDirectory) then
  begin
    IsPatchMode := True;
    SetAccessStatus('در حال آماده‌سازی و به‌روزرسانی هوشمند فایل‌های کلاینت…', clGray);
    SmartPatchTargetDirectory(InstallDirectory);
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

  { Otherwise, if it was an empty/temporary folder, clean it normally }
  SetAccessStatus('در حال پاک‌سازی پوشه انتخاب‌شده Allclient…', clGray);
  if not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'حذف کامل پوشه قبلی ممکن نشد. بازی و لانچر را ببندید و دوباره تلاش کنید.';
    Exit;
  end;

  Result := True;
end;

procedure CheckInstalledHomeSubscription;
var
  Directory, Version: String;
  IdentityFile, StoredPhone: String;
begin
  if not UpdateAccessChecked then
  begin
    UpdateAccessChecked := True;
    if FindPreviousAllclient(Directory, Version) and
       FileExists(AddBackslash(Directory) + 'cstrike.exe') and
       PreviousInstallPathIsSafe(Directory) then
    begin
      DetectedInstallDirectory := Directory;
      IdentityFile := AddBackslash(Directory) + 'allclient-install.ini';
      if FileExists(IdentityFile) then
      begin
        StoredPhone := Trim(GetIniString('Allclient', 'PhoneNumber', '', IdentityFile));
        if (StoredPhone <> '') and (HomePhoneEdit.Text = '') then
        begin
          HomePhoneEdit.Text := StoredPhone;
        end;
      end;
      WizardForm.DirEdit.Text := Directory;
    end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = HomeActivationPage.ID) and not UpdateAccessChecked then
  begin
    CheckInstalledHomeSubscription;
  end;
  if SubscriptionUpdate and (CurPageID = wpReady) then
  begin
    WizardForm.NextButton.Caption := 'به‌روزرسانی';
    WizardForm.ReadyMemo.Text := 'اشتراک کلاینت خانگی (شماره: ' + UserPhoneNumber + ') تأیید شد.' + #13#10 +
      'مسیر به‌روزرسانی: ' + DetectedInstallDirectory + #13#10 +
      'فایل‌ها و پچ جدید کلاینت جایگزین خواهند شد. لطفاً قبل از شروع، بازی و لانچر را ببندید.';
  end;
end;

procedure InitializeWizard;
begin
  DeviceHash24 := Compute24CharDeviceHash();

  HomeActivationPage := CreateCustomPage(
    wpWelcome,
    'فعال‌سازی اشتراک کلاینت خانگی گیم‌لند',
    'شناسه سخت‌افزاری اختصاصی دستگاه و تأیید شماره موبایل');

  HomeDescLabel := TNewStaticText.Create(WizardForm);
  HomeDescLabel.Parent := HomeActivationPage.Surface;
  HomeDescLabel.Left := ScaleX(4);
  HomeDescLabel.Top := ScaleY(4);
  HomeDescLabel.Width := HomeActivationPage.SurfaceWidth - ScaleX(8);
  HomeDescLabel.Height := ScaleY(46);
  HomeDescLabel.AutoSize := False;
  HomeDescLabel.WordWrap := True;
  HomeDescLabel.Caption :=
    'این نسخه مخصوص کلاینت خانگی است. کد دستگاه زیر به صورت هوشمند از قطعات سخت‌افزاری رایانه شما تولید شده است. لطفاً آن را کپی کرده و برای مدیریت ارسال کنید تا با شماره همراه شما فعال شود:';

  HomeHashLabel := TNewStaticText.Create(WizardForm);
  HomeHashLabel.Parent := HomeActivationPage.Surface;
  HomeHashLabel.Left := ScaleX(4);
  HomeHashLabel.Top := ScaleY(54);
  HomeHashLabel.Width := HomeActivationPage.SurfaceWidth - ScaleX(8);
  HomeHashLabel.Height := ScaleY(18);
  HomeHashLabel.Caption := 'کد اختصاصی دستگاه شما (۲۴ کاراکتر - غیرقابل ویرایش):';
  HomeHashLabel.Font.Style := [fsBold];

  HomeHashEdit := TNewEdit.Create(WizardForm);
  HomeHashEdit.Parent := HomeActivationPage.Surface;
  HomeHashEdit.Left := ScaleX(4);
  HomeHashEdit.Top := ScaleY(74);
  HomeHashEdit.Width := HomeActivationPage.SurfaceWidth - ScaleX(140);
  HomeHashEdit.Height := ScaleY(30);
  HomeHashEdit.Text := DeviceHash24;
  HomeHashEdit.ReadOnly := True;
  HomeHashEdit.Color := clBtnFace;
  HomeHashEdit.Font.Name := 'Consolas';
  HomeHashEdit.Font.Size := 11;
  HomeHashEdit.Font.Style := [fsBold];

  HomeCopyButton := TNewButton.Create(WizardForm);
  HomeCopyButton.Parent := HomeActivationPage.Surface;
  HomeCopyButton.Left := HomeActivationPage.SurfaceWidth - ScaleX(130);
  HomeCopyButton.Top := ScaleY(73);
  HomeCopyButton.Width := ScaleX(130);
  HomeCopyButton.Height := ScaleY(32);
  HomeCopyButton.Caption := 'کپی کد دستگاه (Copy)';
  HomeCopyButton.OnClick := @CopyDeviceHashClick;

  HomePhoneLabel := TNewStaticText.Create(WizardForm);
  HomePhoneLabel.Parent := HomeActivationPage.Surface;
  HomePhoneLabel.Left := ScaleX(4);
  HomePhoneLabel.Top := ScaleY(118);
  HomePhoneLabel.Width := HomeActivationPage.SurfaceWidth - ScaleX(8);
  HomePhoneLabel.Height := ScaleY(18);
  HomePhoneLabel.Caption := 'شماره تلفن همراه شما (ثبت‌شده در پنل مدیریت):';
  HomePhoneLabel.Font.Style := [fsBold];

  HomePhoneEdit := TNewEdit.Create(WizardForm);
  HomePhoneEdit.Parent := HomeActivationPage.Surface;
  HomePhoneEdit.Left := ScaleX(4);
  HomePhoneEdit.Top := ScaleY(138);
  HomePhoneEdit.Width := ScaleX(200);
  HomePhoneEdit.Height := ScaleY(26);
  HomePhoneEdit.MaxLength := 15;
  HomePhoneEdit.Text := '';

  HomeVerifyButton := TNewButton.Create(WizardForm);
  HomeVerifyButton.Parent := HomeActivationPage.Surface;
  HomeVerifyButton.Left := ScaleX(215);
  HomeVerifyButton.Top := ScaleY(137);
  HomeVerifyButton.Width := ScaleX(160);
  HomeVerifyButton.Height := ScaleY(28);
  HomeVerifyButton.Caption := 'بررسی و تأیید فعال‌سازی';
  HomeVerifyButton.OnClick := @VerifyHomeClientClick;

  HomeStatusLabel := TNewStaticText.Create(WizardForm);
  HomeStatusLabel.Parent := HomeActivationPage.Surface;
  HomeStatusLabel.Left := ScaleX(4);
  HomeStatusLabel.Top := ScaleY(178);
  HomeStatusLabel.Width := HomeActivationPage.SurfaceWidth - ScaleX(8);
  HomeStatusLabel.Height := ScaleY(52);
  HomeStatusLabel.AutoSize := False;
  HomeStatusLabel.WordWrap := True;
  HomeStatusLabel.Caption := 'پس از ارسال کد بالا به مدیریت، شماره موبایل خود را وارد کرده و روی «بررسی و تأیید فعال‌سازی» کلیک کنید.';
  HomeStatusLabel.Font.Color := clGray;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = HomeActivationPage.ID then
  begin
    if not AccessApproved then
    begin
      WizardForm.NextButton.Enabled := False;
      try
        Result := HomeAccessVerify(HomeHashEdit.Text, HomePhoneEdit.Text);
        if Result then
        begin
          AccessApproved := True;
          HomeStatusLabel.Caption := OnlineVerificationMessage;
          HomeStatusLabel.Font.Color := clGreen;
          if (DetectedInstallDirectory <> '') and (CompareText(WizardForm.DirEdit.Text, DetectedInstallDirectory) = 0) then
            SubscriptionUpdate := True;
        end
        else
        begin
          AccessApproved := False;
          HomeStatusLabel.Caption := OnlineVerificationMessage;
          HomeStatusLabel.Font.Color := clRed;
          MsgBox(OnlineVerificationMessage, mbError, MB_OK);
        end;
      finally
        WizardForm.NextButton.Enabled := True;
      end;
    end;
    Exit;
  end;
end;

function ReadInstalledGameNetTag(const Directory: String; var Tag: String): Boolean;
var
  IdentityFile, RegistryTag: String;
  RawTag: AnsiString;
begin
  Tag := '';
  IdentityFile := AddBackslash(Directory) + 'allclient-install.ini';
  RegQueryStringValue(DetectedInstallRoot, AllclientUninstallKey, 'GameNetTag', Tag);
  RegistryTag := Trim(Tag);

  if FileExists(IdentityFile) then
  begin
    Tag := Trim(GetIniString('Allclient', 'GameNetTag', '', IdentityFile));
  end;

  if Tag = '' then
    Tag := RegistryTag;

  if (Tag = '') and FileExists(AddBackslash(Directory) + 'client_tags.txt') then
  begin
    Tag := Trim(GetIniString('', '', '', AddBackslash(Directory) + 'client_tags.txt'));
    if Tag = '' then
    begin
      RawTag := '';
      if LoadStringFromFile(AddBackslash(Directory) + 'client_tags.txt', RawTag) then
        Tag := Trim(String(RawTag));
    end;
  end;

  Result := (Tag <> '');
end;

procedure CheckInstalledSubscription;
var
  Directory, Version, InstalledTag, Response: String;
begin
  if not UpdateAccessChecked then
  begin
    UpdateAccessChecked := True;
    if FindPreviousAllclient(Directory, Version) and
       FileExists(AddBackslash(Directory) + 'cstrike.exe') and
       PreviousInstallPathIsSafe(Directory) then
    begin
      InstalledTag := '';
      if not ReadInstalledGameNetTag(Directory, InstalledTag) or (InstalledTag = '') then
        InstalledTag := '{#BuildTag}';

      if FetchAccessResponse('http://gameland.cam/update_access.php?tag=' + InstalledTag, Response) then
      begin
        SubscriptionUpdate := (Pos('ACTIVE', Response) = 1);
        if SubscriptionUpdate then
        begin
          ActiveGameNetTag := InstalledTag;
          AccessApproved := True;
          DetectedInstallDirectory := Directory;
          WizardForm.DirEdit.Text := Directory;
        end;
      end;
    end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := SubscriptionUpdate and
    ((PageID = wpSelectDir) or (PageID = wpSelectProgramGroup));
end;

function IsVCRedist2015To2022InstalledX86(): Boolean;
var
  Installed: Cardinal;
begin
  Result := False;
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and RegQueryDWordValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X86', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if FileExists(ExpandConstant('{sys}\vcruntime140.dll')) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and FileExists(ExpandConstant('{syswow64}\vcruntime140.dll')) then
  begin
    Result := True;
    Exit;
  end;
end;

function IsVCRedist2015To2022InstalledX64(): Boolean;
var
  Installed: Cardinal;
begin
  Result := False;
  if not IsWin64 then Exit;
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if FileExists(ExpandConstant('{sys}\vcruntime140.dll')) then
  begin
    Result := True;
    Exit;
  end;
end;

function IsVCRedist2010InstalledX86(): Boolean;
var
  Installed: Cardinal;
begin
  Result := False;
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\10.0\VC\VCRedist\x86', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and RegQueryDWordValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\VC\VCRedist\x86', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if FileExists(ExpandConstant('{sys}\msvcr100.dll')) then
  begin
    Result := True;
    Exit;
  end;
  if IsWin64 and FileExists(ExpandConstant('{syswow64}\msvcr100.dll')) then
  begin
    Result := True;
    Exit;
  end;
end;

function IsVCRedist2010InstalledX64(): Boolean;
var
  Installed: Cardinal;
begin
  Result := False;
  if not IsWin64 then Exit;
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\10.0\VC\VCRedist\x64', 'Installed', Installed) and (Installed = 1) then
  begin
    Result := True;
    Exit;
  end;
  if FileExists(ExpandConstant('{sys}\msvcr100.dll')) then
  begin
    Result := True;
    Exit;
  end;
end;

function InstallRuntime(const FileName, Parameters: String;
  var NeedsRestart: Boolean): Boolean;
var
  Code: Integer;
begin
  Result := True;
  try
    ExtractTemporaryFile(FileName);
    if not ShellExec('runas', ExpandConstant('{tmp}\') + FileName,
      Parameters, '', SW_HIDE, ewWaitUntilTerminated, Code) then
    begin
      { If user declined elevation prompt or policy blocked it, log and continue safely }
      Log('ShellExec runas for ' + FileName + ' returned false.');
      Exit;
    end;

    if (Code = 3010) or (Code = 1641) then NeedsRestart := True;

    { Recognized exit codes:
      0: Success
      3010: ERROR_SUCCESS_REBOOT_REQUIRED
      1641: ERROR_SUCCESS_REBOOT_INITIATED
      1638: ERROR_PRODUCT_VERSION (A newer version is already installed)
      5100: ERROR_NEWER_PRODUCT_OR_SRV_PACK (Newer version already installed)
      -2147023258: WiX Burn 0x80070666 ERROR_PRODUCT_VERSION
      1618: ERROR_INSTALL_ALREADY_RUNNING
      1602: ERROR_INSTALL_USEREXIT
    }
    Log(Format('Runtime %s finished with exit code %d.', [FileName, Code]));
  except
    Log('Exception during execution of ' + FileName);
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  if not AccessApproved then
  begin
    Result := 'برای ادامه، ابتدا کد نصب را تأیید کنید.';
    Exit;
  end;

  if SubscriptionUpdate and
     (CompareText(RemoveBackslashUnlessRoot(ExpandConstant('{app}')),
       RemoveBackslashUnlessRoot(DetectedInstallDirectory)) <> 0) then
  begin
    Result := 'مسیر به‌روزرسانی با نسخه شناسایی‌شده مطابقت ندارد. نصب‌کننده را دوباره اجرا کنید.';
    Exit;
  end;

  if not DependenciesReady then
  begin
    SetAccessStatus('در حال بررسی و آماده‌سازی پیش‌نیازها…', clGray);

    { 1. Visual C++ 2015-2022 (x86) }
    if IsVCRedist2015To2022InstalledX86 then
      Log('Visual C++ 2015-2022 (x86) is already installed. Skipping.')
    else
    begin
      SetAccessStatus('در حال نصب پیش‌نیاز Visual C++ (x86)…', clGray);
      InstallRuntime('vc_redist.x86.exe', '/install /quiet /norestart', NeedsRestart);
    end;

    { 2. Visual C++ 2010 (x86) }
    if IsVCRedist2010InstalledX86 then
      Log('Visual C++ 2010 (x86) is already installed. Skipping.')
    else
    begin
      SetAccessStatus('در حال نصب پیش‌نیاز Visual C++ 2010 (x86)…', clGray);
      InstallRuntime('vcredist2010_x86.exe', '/q /norestart', NeedsRestart);
    end;

    { 3. Visual C++ 2015-2022 (x64) on 64-bit Windows }
    if IsWin64 then
    begin
      if IsVCRedist2015To2022InstalledX64 then
        Log('Visual C++ 2015-2022 (x64) is already installed. Skipping.')
      else
        InstallRuntime('vc_redist.x64.exe', '/install /quiet /norestart', NeedsRestart);
    end;

    { 4. Visual C++ 2010 (x64) on 64-bit Windows }
    if IsWin64 then
    begin
      if IsVCRedist2010InstalledX64 then
        Log('Visual C++ 2010 (x64) is already installed. Skipping.')
      else
        InstallRuntime('vcredist2010_x64.exe', '/q /norestart', NeedsRestart);
    end;

    { Allclient core components are compiled with /MT (Static CRT) and do not hard-depend
      on external DLLs. Set DependenciesReady to True so installation never stalls. }
    DependenciesReady := True;
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

procedure ExtractZip(const ZipFile, TargetFolder: String);
var
  Shell, ZipFolder, Target: Variant;
begin
  Shell := CreateOleObject('Shell.Application');
  ZipFolder := Shell.NameSpace(ZipFile);
  Target := Shell.NameSpace(TargetFolder);
  if (not VarIsClear(ZipFolder)) and (not VarIsClear(Target)) then
    Target.CopyHere(ZipFolder.Items, 4 + 16);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    { AntiCopy Hardware Lock: Always ensure InstallID is registered on the target machine }
    RegWriteStringValue(HKCU, 'Software\NextClient', 'InstallID', GetHardwareID(''));
    if IsWin64 then
      RegWriteStringValue(HKLM64, 'Software\NextClient', 'InstallID', GetHardwareID(''))
    else
      RegWriteStringValue(HKLM32, 'Software\NextClient', 'InstallID', GetHardwareID(''));


    SetIniString('Allclient', 'ClientType', 'Home', ExpandConstant('{app}\allclient-install.ini'));
    SetIniString('Allclient', 'GameNetTag', '', ExpandConstant('{app}\allclient-install.ini'));
    SetIniString('Allclient', 'DeviceHash', DeviceHash24, ExpandConstant('{app}\allclient-install.ini'));
    SetIniString('Allclient', 'PhoneNumber', UserPhoneNumber, ExpandConstant('{app}\allclient-install.ini'));
    ConfigureSmartEmu(ExpandConstant('{app}\platform\steam\games\SmartEmu\config.xml'));
  end;
end;

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
  #define SourceRoot "D:\Allclient"
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
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "cstrike\maps\*,cstrike\userconfig.cfg,backups\*,cstrike_downloads\*,crashes\*,htmlcache\*,*.log,*.mdmp,debug.log,install.bat,unins000.exe,unins000.dat,*.bak*,hitbox_vis.asi*,*.asi.disabled,auto_launcher_tests.exe"
; 2. Game maps - NEVER overwrite existing maps! Custom and downloaded maps are 100% preserved
Source: "{#SourceRoot}\cstrike\maps\*"; DestDir: "{app}\cstrike\maps"; Flags: onlyifdoesntexist recursesubdirs createallsubdirs; Excludes: "*.log,*.bak*"
; 3. User config template - only install if not already existing
Source: "{#SourceRoot}\cstrike\userconfig.cfg"; DestDir: "{app}\cstrike"; Flags: onlyifdoesntexist;
; 4. Overlay latest compiled binaries and configs
Source: "{#BinaryRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "cstrike\maps\*,*.log,*.mdmp,debug.log,hitbox_vis.asi*,*.asi.disabled,auto_launcher_tests.exe"

[INI]
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "Schema"; String: "1"
Filename: "{app}\allclient-install.ini"; Section: "Allclient"; Key: "GameNetTag"; String: "{code:GetActiveGameNetTag}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{{D9E46BD1-52F8-470F-8639-FF31FE7C5E48}_is1"; ValueType: string; ValueName: "GameNetTag"; ValueData: "{code:GetActiveGameNetTag}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\NextClient"; ValueType: string; ValueName: "InstallID"; ValueData: "{code:GetHardwareID}"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\NextClient"; ValueType: string; ValueName: "InstallID"; ValueData: "{code:GetHardwareID}"; Flags: uninsdeletevalue noerror

[Icons]
Name: "{autodesktop}\Allclient"; Filename: "{app}\Allclient.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Allclient.exe"
Name: "{group}\Ø­Ø°Ù Allclient"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Allclient.exe"; WorkingDir: "{app}"; Description: "Ø§Ø¬Ø±Ø§ÛŒ Allclient"; Flags: nowait postinstall skipifsilent unchecked

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
    MsgBox('Ø®Ø·Ø§ÛŒ Ø§Ù…Ù†ÛŒØªÛŒ: ÙØ§ÛŒÙ„ Ø¯Ø§Ø¯Ù‡â€ŒÙ‡Ø§ÛŒ Ø¨Ø§Ø²ÛŒ (Allclient-Setup-1.bin) Ø¯Ø± Ú©Ù†Ø§Ø± Ø¨Ø±Ù†Ø§Ù…Ù‡ Ù†ØµØ¨ ÛŒØ§ÙØª Ù†Ø´Ø¯.' + #13#10#13#10 +
           'Ù„Ø·ÙØ§Ù‹ Ù‡Ø± Ø¯Ùˆ ÙØ§ÛŒÙ„ Allclient-Setup.exe Ùˆ Allclient-Setup-1.bin Ø±Ø§ Ø¯Ø± ÛŒÚ© Ù¾ÙˆØ´Ù‡ Ù‚Ø±Ø§Ø± Ø¯Ù‡ÛŒØ¯.', mbCriticalError, MB_OK);
    Result := False;
    Exit;
  end;

  if not FileSize(BinSlice, BinSize) or (BinSize < 100000000) then
  begin
    MsgBox('Ø®Ø·Ø§ÛŒ Ø§Ù…Ù†ÛŒØªÛŒ: ÙØ§ÛŒÙ„ Ø¯Ø§Ø¯Ù‡â€ŒÙ‡Ø§ÛŒ Ø¨Ø§Ø²ÛŒ (Allclient-Setup-1.bin) Ù†Ø§Ù‚Øµ ÛŒØ§ Ø¯Ø³ØªÚ©Ø§Ø±ÛŒ Ø´Ø¯Ù‡ Ø§Ø³Øª.' + #13#10#13#10 +
           'Ø­Ø¬Ù… ÙØ§ÛŒÙ„ Ù…Ø¹ØªØ¨Ø± Ù†ÛŒØ³Øª. Ù„Ø·ÙØ§Ù‹ Ù…Ø¬Ø¯Ø¯Ø§Ù‹ ÙØ§ÛŒÙ„ Ú©Ø§Ù…Ù„ Ø±Ø§ Ø¯Ø±ÛŒØ§ÙØª ÙØ±Ù…Ø§ÛŒÛŒØ¯.', mbCriticalError, MB_OK);
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
  SubscriptionUpdate: Boolean;
  DetectedInstallDirectory: String;
  DetectedInstallRoot: Integer;
  DependenciesReady: Boolean;
  PayloadDownloadUrl: String;
  PayloadDownloadPage: TDownloadWizardPage;
  ActiveGameNetTag: String;
  IsPatchMode: Boolean;

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
    OnlineVerificationMessage := 'Ù†Ø´Ø§Ù†ÛŒ Ø³Ø±ÙˆÛŒØ³ ØªØ£ÛŒÛŒØ¯ Ù…Ø¹ØªØ¨Ø± Ù†ÛŒØ³Øª.';
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
    SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ø§ØªØµØ§Ù„ Ø¨Ù‡ Ø³Ø±ÙˆÛŒØ³Ø› ØªÙ„Ø§Ø´ ' +
      IntToStr(Attempt) + ' Ø§Ø² Û²â€¦', clGray);
    try
      if FetchWithInternetExplorer(Url, ResponseText) then
      begin
        Result := True;
        Exit;
      end
      else
      begin
        SetAccessStatus('Ø§ØªØµØ§Ù„ Ø¨Ø±Ù‚Ø±Ø§Ø± Ù†Ø´Ø¯Ø› Ø¯Ø± Ø­Ø§Ù„ Ø¨Ø±Ø±Ø³ÛŒ Ø±ÙˆØ´ Ø¬Ø§ÛŒÚ¯Ø²ÛŒÙ†â€¦', clGray);
        if FetchWithSetupDownloader(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;

        SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ ØªÙ„Ø§Ø´ Ø¨Ø§ Ø±ÙˆØ´ Ø³Ø§Ø²Ú¯Ø§Ø± Ø¨Ø§ ÙˆÛŒÙ†Ø¯ÙˆØ²â€¦', clGray);
        if FetchWithNativeRequest(Url, ResponseText) then
        begin
          Result := True;
          Exit;
        end;

      end;

      if Attempt < 2 then
        SetAccessStatus('Ø§Ø±ØªØ¨Ø§Ø· Ù‚Ø·Ø¹ Ø´Ø¯Ø› Ø¯Ø± Ø­Ø§Ù„ ØªÙ„Ø§Ø´ Ø¯ÙˆØ¨Ø§Ø±Ù‡â€¦', clGray);
    except
      OnlineVerificationMessage := 'Ù¾Ø§Ø³Ø® Ø³Ø±ÙˆÛŒØ³ Ù‚Ø§Ø¨Ù„ Ù¾Ø±Ø¯Ø§Ø²Ø´ Ù†ÛŒØ³Øª.';
      Result := False;
    end;
  end;
end;

function FetchAccessApi(const Query: String; var ResponseText: String): Boolean;
var
  NormalizedResponse: String;
begin
  SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ø§ØªØµØ§Ù„ Ø¨Ù‡ Ø³Ø±ÙˆÛŒØ³ ØªØ£ÛŒÛŒØ¯ Allclientâ€¦', clGray);
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

function OnlineAccessCodeIsValid(const Username, Password: String): Boolean;
var
  RequestUrl: String;
  ResponseText: String;
  NormalizedResponse: String;
begin
  Result := False;
  OnlineServiceUnavailable := False;
  OnlineVerificationMessage := '';
  PayloadDownloadUrl := '';

  if Length(Trim(Username)) = 0 then
  begin
    OnlineVerificationMessage := 'Ù†Ø§Ù… Ú©Ø§Ø±Ø¨Ø±ÛŒ Ù†Ù…ÛŒâ€ŒØªÙˆØ§Ù†Ø¯ Ø®Ø§Ù„ÛŒ Ø¨Ø§Ø´Ø¯.';
    Exit;
  end;

  if Length(Trim(Password)) = 0 then
  begin
    OnlineVerificationMessage := 'Ø±Ù…Ø² Ø¹Ø¨ÙˆØ± Ù†Ù…ÛŒâ€ŒØªÙˆØ§Ù†Ø¯ Ø®Ø§Ù„ÛŒ Ø¨Ø§Ø´Ø¯.';
    Exit;
  end;


  SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ø¨Ø±Ø±Ø³ÛŒ Ø§Ø·Ù„Ø§Ø¹Ø§Øª ÙˆØ§Ø±Ø¯Ø´Ø¯Ù‡â€¦', clGray);
  RequestUrl := '?action=verify&username=' + Trim(Username) + '&password=' + Trim(Password);
  if FetchAccessApi(RequestUrl, ResponseText) then
  begin
    SetAccessStatus('Ù¾Ø§Ø³Ø® Ø¯Ø±ÛŒØ§ÙØª Ø´Ø¯Ø› Ø¯Ø± Ø­Ø§Ù„ Ø¨Ø±Ø±Ø³ÛŒ Ù†ØªÛŒØ¬Ù‡â€¦', clGray);
    NormalizedResponse := Lowercase(ResponseText);
    if Pos('"valid":true', NormalizedResponse) > 0 then
    begin
      Result := True;
      ActiveGameNetTag := Trim(Username);
      OnlineVerificationMessage := 'Ù„Ø§ÛŒØ³Ù†Ø³ Ø¢Ù†Ù„Ø§ÛŒÙ† ØªØ£ÛŒÛŒØ¯ Ø´Ø¯.';
      PayloadDownloadUrl := GetJsonString(ResponseText, 'download_url');
      if PayloadDownloadUrl <> '' then
        PayloadDownloadUrl := Copy(PayloadDownloadUrl, 1, Length(PayloadDownloadUrl)); // Clean copy
    end
    else if Pos('"valid":false', NormalizedResponse) > 0 then
      OnlineVerificationMessage := 'Ø§Ø·Ù„Ø§Ø¹Ø§Øª ÙˆØ±ÙˆØ¯ Ø§Ø´ØªØ¨Ø§Ù‡ Ø§Ø³Øª ÛŒØ§ Ø§Ø´ØªØ±Ø§Ú© Ù„ØºÙˆ Ø´Ø¯Ù‡ Ø§Ø³Øª.'
    else
    begin
      OnlineServiceUnavailable := True;
      OnlineVerificationMessage := 'Ù¾Ø§Ø³Ø® Ø³Ø±ÙˆÛŒØ³ Ø¢Ù†Ù„Ø§ÛŒÙ† Ù…Ø¹ØªØ¨Ø± Ù†ÛŒØ³Øª.';
    end;
  end
  else
  begin
    OnlineServiceUnavailable := True;
    if OnlineVerificationMessage = '' then
      OnlineVerificationMessage := 'Ø³Ø±ÙˆÛŒØ³ ØªØ£ÛŒÛŒØ¯ Ø¢Ù†Ù„Ø§ÛŒÙ† Ø¯Ø± Ø¯Ø³ØªØ±Ø³ Ù†ÛŒØ³Øª.';
  end;
end;

function AccessCodeIsValid(const Username, Password: String): Boolean;
begin
  Result := CompareText(Trim(Password), OfflineCode) = 0;
  if Result then
    OnlineVerificationMessage := 'Ú©Ø¯ Ø¯Ø³ØªØ±Ø³ÛŒ Ø¢ÙÙ„Ø§ÛŒÙ† ØªØ£ÛŒÛŒØ¯ Ø´Ø¯.'
  else
    Result := OnlineAccessCodeIsValid(Username, Password);
end;

procedure RefreshAccessStatus(Sender: TObject);
var
  ResponseText: String;
begin
  RefreshAccessButton.Enabled := False;
  OnlineVerificationMessage := '';
  SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ø¨Ø±Ø±Ø³ÛŒ Ø§ØªØµØ§Ù„â€¦', clGray);
  try
    if FetchAccessApi('?action=status', ResponseText) and
       (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
      SetAccessStatus('Ø³Ø±ÙˆÛŒØ³ Ø¢Ù†Ù„Ø§ÛŒÙ† Ù…ØªØµÙ„ Ø§Ø³Øª Ùˆ Ù¾Ø§Ø³Ø® Ù…ÛŒâ€ŒØ¯Ù‡Ø¯.', clGreen)
    else
    begin
      if OnlineVerificationMessage = '' then
        OnlineVerificationMessage := 'ÙˆØ¶Ø¹ÛŒØª Ù…Ø¹ØªØ¨Ø±ÛŒ Ø§Ø² Ø³Ø±ÙˆÛŒØ³ Ø¯Ø±ÛŒØ§ÙØª Ù†Ø´Ø¯.';
      SetAccessStatus('Ø³Ø±ÙˆÛŒØ³ Ø¯Ø± Ø¯Ø³ØªØ±Ø³ Ù†ÛŒØ³Øª: ' + OnlineVerificationMessage, clRed);
    end;
  except
    SetAccessStatus('Ø¨Ø±Ø±Ø³ÛŒ Ø§ØªØµØ§Ù„ Ø¨Ø§ Ø®Ø·Ø§ Ù…ÙˆØ§Ø¬Ù‡ Ø´Ø¯Ø› Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯.', clRed);
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
    'Ø¯Ø± Ø­Ø§Ù„ Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ø§ØªØµØ§Ù„ Ø¨Ø±Ø§ÛŒ ØªØ£ÛŒÛŒØ¯ Ø¢Ù†Ù„Ø§ÛŒÙ†â€¦';
  WizardForm.Update;

  try
    PreparationProgress.Position := 55;
    PreparationStatusLabel.Caption :=
      'Ø§ØªØµØ§Ù„ Ø¢Ù…Ø§Ø¯Ù‡ Ø§Ø³ØªØ› Ø¯Ø± Ø­Ø§Ù„ Ø¨Ø±Ø±Ø³ÛŒ Ø¯Ø³ØªØ±Ø³ÛŒ Ø¢Ù†Ù„Ø§ÛŒÙ†â€¦';
    WizardForm.Update;

    if FetchAccessApi('?action=status', ResponseText) and
       (Pos('"service":"allclient-access"', Lowercase(ResponseText)) > 0) then
    begin
      PreparationProgress.Position := 100;
      PreparationReady := True;
      PreparationStatusLabel.Font.Color := clGreen;
      PreparationStatusLabel.Caption :=
        'Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ú©Ø§Ù…Ù„ Ø´Ø¯Ø› ØªØ£ÛŒÛŒØ¯ Ø¢Ù†Ù„Ø§ÛŒÙ† Ø¢Ù…Ø§Ø¯Ù‡ Ø§Ø³Øª.';
    end
    else
    begin
      PreparationProgress.Position := 55;
      PreparationStatusLabel.Font.Color := clRed;
      PreparationStatusLabel.Caption :=
        'Ø³Ø±ÙˆÛŒØ³ Ø¢Ù†Ù„Ø§ÛŒÙ† Ù¾Ø§Ø³Ø® Ù†Ø¯Ø§Ø¯Ø› Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯ ÛŒØ§ Ø¨Ø§ Ú©Ø¯ Ø¢ÙÙ„Ø§ÛŒÙ† Ø§Ø¯Ø§Ù…Ù‡ Ø¯Ù‡ÛŒØ¯.';
      PreparationRetryButton.Visible := True;
    end;
  except
    PreparationProgress.Position := 0;
    PreparationStatusLabel.Font.Color := clRed;
    PreparationStatusLabel.Caption :=
      'Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ø§Ù†Ø¬Ø§Ù… Ù†Ø´Ø¯Ø› Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯ ÛŒØ§ Ø¨Ø§ Ú©Ø¯ Ø¢ÙÙ„Ø§ÛŒÙ† Ø§Ø¯Ø§Ù…Ù‡ Ø¯Ù‡ÛŒØ¯.';
    PreparationRetryButton.Visible := True;
  end;

  WizardForm.NextButton.Enabled := True;
  WizardForm.Update;
end;

procedure CheckInstalledSubscription; forward;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = PreparationPage.ID) and not PreparationStarted then
  begin
    CheckInstalledSubscription;
    if SubscriptionUpdate then
    begin
      PreparationStarted := True;
      PreparationReady := True;
      PreparationProgress.Position := 100;
      PreparationStatusLabel.Font.Color := clGreen;
      PreparationStatusLabel.Caption := 'Ú©Ù„Ø§ÛŒÙ†Øª Ù‚Ø¨Ù„ÛŒ Ø¨Ø§ Ø¨Ø±Ú†Ø³Ø¨ Â«' + ActiveGameNetTag + 'Â» Ø´Ù†Ø§Ø³Ø§ÛŒÛŒ Ø´Ø¯ Ùˆ Ø§Ø´ØªØ±Ø§Ú© ÙØ¹Ø§Ù„ Ø§Ø³Øª.' + #13#10 +
        'Ø¨Ø±Ø§ÛŒ Ø¨Ù‡â€ŒØ±ÙˆØ²Ø±Ø³Ø§Ù†ÛŒ Ø®ÙˆØ¯Ú©Ø§Ø±ØŒ Â«Ø¨Ø¹Ø¯ÛŒÂ» Ø±Ø§ Ø¨Ø²Ù†ÛŒØ¯:' + #13#10 + DetectedInstallDirectory;
    end
    else
      RunEarlyPreparation(nil);
  end;
  if SubscriptionUpdate and (CurPageID = wpReady) then
  begin
    WizardForm.NextButton.Caption := 'Ø¨Ù‡â€ŒØ±ÙˆØ²Ø±Ø³Ø§Ù†ÛŒ';
    WizardForm.ReadyMemo.Text := 'Ø§Ø´ØªØ±Ø§Ú© Ú¯ÛŒÙ…â€ŒÙ†Øª Â«' + ActiveGameNetTag + 'Â» ØªØ£ÛŒÛŒØ¯ Ø´Ø¯Ø› Ù†ÛŒØ§Ø²ÛŒ Ø¨Ù‡ ÙˆØ±ÙˆØ¯ Ù…Ø´Ø®ØµØ§Øª Ù†ÛŒØ³Øª.' + #13#10 +
      'Ù…Ø³ÛŒØ± Ø¨Ù‡â€ŒØ±ÙˆØ²Ø±Ø³Ø§Ù†ÛŒ: ' + DetectedInstallDirectory + #13#10 +
      'ÙØ§ÛŒÙ„â€ŒÙ‡Ø§ Ùˆ Ù¾Ú† Ø¬Ø¯ÛŒØ¯ Ú©Ù„Ø§ÛŒÙ†Øª Ø¬Ø§ÛŒÚ¯Ø²ÛŒÙ† Ø®ÙˆØ§Ù‡Ù†Ø¯ Ø´Ø¯. Ù„Ø·ÙØ§Ù‹ Ø¨Ø§Ø²ÛŒ Ùˆ Ù„Ø§Ù†Ú†Ø± Ø±Ø§ Ø¨Ø¨Ù†Ø¯ÛŒØ¯.';
  end;
end;

procedure InitializeWizard;
begin
  PreparationPage := CreateCustomPage(
    wpWelcome,
    'Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Allclient',
    'Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ø§ÙˆÙ„ÛŒÙ‡ Ø§ØªØµØ§Ù„');

  PreparationStatusLabel := TNewStaticText.Create(WizardForm);
  PreparationStatusLabel.Parent := PreparationPage.Surface;
  PreparationStatusLabel.Left := ScaleX(12);
  PreparationStatusLabel.Top := ScaleY(34);
  PreparationStatusLabel.Width := PreparationPage.SurfaceWidth - ScaleX(24);
  PreparationStatusLabel.Height := ScaleY(54);
  PreparationStatusLabel.AutoSize := False;
  PreparationStatusLabel.WordWrap := True;
  PreparationStatusLabel.Caption :=
    'Ø¨Ø±Ø§ÛŒ Ø¨Ø±Ø±Ø³ÛŒ Ø§ØªØµØ§Ù„ Ø¢Ù†Ù„Ø§ÛŒÙ† Ø¢Ù…Ø§Ø¯Ù‡ Ø§Ø³Øª.';
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
  PreparationRetryButton.Caption := 'ØªÙ„Ø§Ø´ Ø¯ÙˆØ¨Ø§Ø±Ù‡';
  PreparationRetryButton.OnClick := @RunEarlyPreparation;
  PreparationRetryButton.Visible := False;

  AccessPage := CreateInputQueryPage(
    PreparationPage.ID,
    'ØªØ£ÛŒÛŒØ¯ Ù…Ø¬ÙˆØ² Ù†ØµØ¨',
    'Ø§Ø·Ù„Ø§Ø¹Ø§Øª Ú¯ÛŒÙ…â€ŒÙ†Øª Ø±Ø§ ÙˆØ§Ø±Ø¯ Ú©Ù†ÛŒØ¯',
    'Ù†Ø§Ù… Ú©Ø§Ø±Ø¨Ø±ÛŒ Ùˆ Ø±Ù…Ø² Ø¹Ø¨ÙˆØ± Ø§Ø®ØªØµØ§ØµÛŒ Ú¯ÛŒÙ…â€ŒÙ†Øª Ø®ÙˆØ¯ Ø±Ø§ ÙˆØ§Ø±Ø¯ Ú©Ù†ÛŒØ¯.');
  AccessPage.Add('Ù†Ø§Ù… Ú©Ø§Ø±Ø¨Ø±ÛŒ (Ú©Ø¯ Ø´Ø¹Ø¨Ù‡):', False);
  AccessPage.Add('Ø±Ù…Ø² Ø¹Ø¨ÙˆØ± Ù†ØµØ¨ Ú©Ù„Ø§ÛŒÙ†Øª:', True);

  AccessStatusLabel := TNewStaticText.Create(WizardForm);
  AccessStatusLabel.Parent := AccessPage.Surface;
  AccessStatusLabel.Left := AccessPage.Edits[1].Left;
  AccessStatusLabel.Top := AccessPage.Edits[1].Top + AccessPage.Edits[1].Height + ScaleY(20);
  AccessStatusLabel.Width := AccessPage.SurfaceWidth;
  AccessStatusLabel.Height := ScaleY(42);
  AccessStatusLabel.AutoSize := False;
  AccessStatusLabel.WordWrap := True;
  AccessStatusLabel.Caption := 'Ø§ØªØµØ§Ù„ Ø¢Ù†Ù„Ø§ÛŒÙ† Ù‡Ù†ÙˆØ² Ø¨Ø±Ø±Ø³ÛŒ Ù†Ø´Ø¯Ù‡ Ø§Ø³Øª.';
  AccessStatusLabel.Font.Color := clGray;

  RefreshAccessButton := TNewButton.Create(WizardForm);
  RefreshAccessButton.Parent := AccessPage.Surface;
  RefreshAccessButton.Left := AccessPage.Edits[1].Left;
  RefreshAccessButton.Top := AccessStatusLabel.Top + AccessStatusLabel.Height + ScaleY(10);
  RefreshAccessButton.Width := ScaleX(150);
  RefreshAccessButton.Height := ScaleY(30);
  RefreshAccessButton.Caption := 'Ø¨Ø±Ø±Ø³ÛŒ Ø¯ÙˆØ¨Ø§Ø±Ù‡ Ø§ØªØµØ§Ù„';
  RefreshAccessButton.OnClick := @RefreshAccessStatus;

  PayloadDownloadPage := CreateDownloadPage('Ø¯Ø± Ø­Ø§Ù„ Ø¯Ø±ÛŒØ§ÙØª Ø§Ø·Ù„Ø§Ø¹Ø§Øª Ú¯ÛŒÙ…â€ŒÙ†Øª', 'Ù„Ø·ÙØ§Ù‹ Ù…Ù†ØªØ¸Ø± Ø¨Ù…Ø§Ù†ÛŒØ¯...', nil);
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
        'Ø¨Ø¹Ø¶ÛŒ ÙØ§ÛŒÙ„â€ŒÙ‡Ø§ÛŒ Ù†Ø³Ø®Ù‡ Ù‚Ø¨Ù„ÛŒ Ø¯Ø± Ø­Ø§Ù„ Ø§Ø³ØªÙØ§Ø¯Ù‡â€ŒØ§Ù†Ø¯. Ø¨Ø§Ø²ÛŒ Ùˆ Ù„Ø§Ù†Ú†Ø± Ø±Ø§ Ø¨Ø¨Ù†Ø¯ÛŒØ¯ Ùˆ Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯.';
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
      'Ù†Ø³Ø®Ù‡ Ù‚Ø¨Ù„ÛŒ Ù¾ÛŒØ¯Ø§ Ø´Ø¯ØŒ Ø§Ù…Ø§ Ù¾ÙˆØ´Ù‡ Ø«Ø¨Øªâ€ŒØ´Ø¯Ù‡ Ø¢Ù† Ø¨Ø±Ø§ÛŒ Ø­Ø°Ù Ø®ÙˆØ¯Ú©Ø§Ø± Ù…Ù†Ø§Ø³Ø¨ Ù†ÛŒØ³Øª.';
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
      'Ù¾ÙˆØ´Ù‡ Ø§Ù†ØªØ®Ø§Ø¨â€ŒØ´Ø¯Ù‡ Ø¨Ø±Ø§ÛŒ Ù¾Ø§Ú©â€ŒØ³Ø§Ø²ÛŒ Ø®ÙˆØ¯Ú©Ø§Ø± Ù…Ù†Ø§Ø³Ø¨ Ù†ÛŒØ³Øª.';
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
    SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ø¢Ù…Ø§Ø¯Ù‡â€ŒØ³Ø§Ø²ÛŒ Ùˆ Ø¨Ù‡â€ŒØ±ÙˆØ²Ø±Ø³Ø§Ù†ÛŒ Ù‡ÙˆØ´Ù…Ù†Ø¯ ÙØ§ÛŒÙ„â€ŒÙ‡Ø§ÛŒ Ú©Ù„Ø§ÛŒÙ†Øªâ€¦', clGray);
    SmartPatchTargetDirectory(InstallDirectory);
    Result := True;
    Exit;
  end;

  if DirectoryHasEntries(InstallDirectory) and
     not DirectoryLooksLikeAllclient(InstallDirectory) then
  begin
    ErrorMessage :=
      'Ù¾ÙˆØ´Ù‡ Ø´Ø§Ù…Ù„ ÙØ§ÛŒÙ„â€ŒÙ‡Ø§ÛŒ Ù†Ø§Ù…Ø±ØªØ¨Ø· Ø§Ø³Øª Ùˆ Ø­Ø°Ù Ù†Ø´Ø¯. Ù¾ÙˆØ´Ù‡ Ø®Ø§Ù„ÛŒ ÛŒØ§ Ù¾ÙˆØ´Ù‡ Ù†Ø³Ø®Ù‡ Ù‚Ø¨Ù„ÛŒ Allclient Ø±Ø§ Ø§Ù†ØªØ®Ø§Ø¨ Ú©Ù†ÛŒØ¯.';
    Exit;
  end;

  { Otherwise, if it was an empty/temporary folder, clean it normally }
  SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ù¾Ø§Ú©â€ŒØ³Ø§Ø²ÛŒ Ù¾ÙˆØ´Ù‡ Ø§Ù†ØªØ®Ø§Ø¨â€ŒØ´Ø¯Ù‡ Allclientâ€¦', clGray);
  if not DelTree(InstallDirectory, True, True, True) then
  begin
    ErrorMessage :=
      'Ø­Ø°Ù Ú©Ø§Ù…Ù„ Ù¾ÙˆØ´Ù‡ Ù‚Ø¨Ù„ÛŒ Ù…Ù…Ú©Ù† Ù†Ø´Ø¯. Ø¨Ø§Ø²ÛŒ Ùˆ Ù„Ø§Ù†Ú†Ø± Ø±Ø§ Ø¨Ø¨Ù†Ø¯ÛŒØ¯ Ùˆ Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯.';
    Exit;
  end;

  Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  InputUsername, InputPassword: String;
begin
  Result := True;
  if CurPageID = AccessPage.ID then
  begin
    WizardForm.NextButton.Enabled := False;
    SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ ØªØ£ÛŒÛŒØ¯ Ù…Ø¬ÙˆØ² Ù†ØµØ¨â€¦', clGray);
    InputUsername := AccessPage.Values[0];
    InputPassword := NormalizeAccessCode(AccessPage.Values[1]);
    try
      try
        Result := AccessCodeIsValid(InputUsername, InputPassword);
      except
        Result := False;
        OnlineServiceUnavailable := True;
        OnlineVerificationMessage := 'ØªØ£ÛŒÛŒØ¯ Ø¨Ø§ Ø®Ø·Ø§ÛŒ ØºÛŒØ±Ù…Ù†ØªØ¸Ø±Ù‡ Ù…ØªÙˆÙ‚Ù Ø´Ø¯Ø› Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯.';
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
          OnlineVerificationMessage := 'Ø§Ø·Ù„Ø§Ø¹Ø§Øª Ù†ØµØ¨ ØªØ£ÛŒÛŒØ¯ Ù†Ø´Ø¯.';
        SetAccessStatus(OnlineVerificationMessage, clRed);
        MsgBox(OnlineVerificationMessage, mbError, MB_OK);
        WizardForm.ActiveControl := AccessPage.Edits[0];
      end;
    finally
      WizardForm.NextButton.Enabled := True;
    end;
  end;

  if CurPageID = wpReady then
  begin
    if PayloadDownloadUrl <> '' then
    begin
      PayloadDownloadPage.Clear;
      PayloadDownloadPage.Add(PayloadDownloadUrl, 'gameland_license.dat', '');
      PayloadDownloadPage.Show;
      try
        try
          PayloadDownloadPage.Download;
          Result := True;
        except
          if PayloadDownloadPage.AbortedByUser then
            Log('Aborted by user.')
          else
            MsgBox('Ø®Ø·Ø§ Ø¯Ø± Ø¯Ø§Ù†Ù„ÙˆØ¯ ÙØ§ÛŒÙ„ Ø§Ø®ØªØµØ§ØµÛŒ Ú¯ÛŒÙ…â€ŒÙ†Øª. Ù„Ø·ÙØ§Ù‹ Ø§ØªØµØ§Ù„ Ø§ÛŒÙ†ØªØ±Ù†Øª Ø±Ø§ Ø¨Ø±Ø±Ø³ÛŒ Ú©Ù†ÛŒØ¯.', mbError, MB_OK);
          Result := False;
        end;
      finally
        PayloadDownloadPage.Hide;
      end;
    end;
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
    ((PageID = AccessPage.ID) or (PageID = wpSelectDir) or (PageID = wpSelectProgramGroup));
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
    Result := 'Ø¨Ø±Ø§ÛŒ Ø§Ø¯Ø§Ù…Ù‡ØŒ Ø§Ø¨ØªØ¯Ø§ Ú©Ø¯ Ù†ØµØ¨ Ø±Ø§ ØªØ£ÛŒÛŒØ¯ Ú©Ù†ÛŒØ¯.';
    Exit;
  end;

  if SubscriptionUpdate and
     (CompareText(RemoveBackslashUnlessRoot(ExpandConstant('{app}')),
       RemoveBackslashUnlessRoot(DetectedInstallDirectory)) <> 0) then
  begin
    Result := 'Ù…Ø³ÛŒØ± Ø¨Ù‡â€ŒØ±ÙˆØ²Ø±Ø³Ø§Ù†ÛŒ Ø¨Ø§ Ù†Ø³Ø®Ù‡ Ø´Ù†Ø§Ø³Ø§ÛŒÛŒâ€ŒØ´Ø¯Ù‡ Ù…Ø·Ø§Ø¨Ù‚Øª Ù†Ø¯Ø§Ø±Ø¯. Ù†ØµØ¨â€ŒÚ©Ù†Ù†Ø¯Ù‡ Ø±Ø§ Ø¯ÙˆØ¨Ø§Ø±Ù‡ Ø§Ø¬Ø±Ø§ Ú©Ù†ÛŒØ¯.';
    Exit;
  end;

  if not DependenciesReady then
  begin
    SetAccessStatus('Ø¯Ø± Ø­Ø§Ù„ Ù†ØµØ¨ Ù¾ÛŒØ´â€ŒÙ†ÛŒØ§Ø²Ù‡Ø§Ø› Ø¯Ø±Ø®ÙˆØ§Ø³Øª Ø¯Ø³ØªØ±Ø³ÛŒ Ù…Ø¯ÛŒØ± ÙˆÛŒÙ†Ø¯ÙˆØ² Ø±Ø§ ØªØ£ÛŒÛŒØ¯ Ú©Ù†ÛŒØ¯.', clGray);
    DependenciesReady := InstallRuntime('vc_redist.x86.exe', '/install /quiet /norestart', NeedsRestart);
    if DependenciesReady then
      DependenciesReady := InstallRuntime('vcredist2010_x86.exe', '/q /norestart', NeedsRestart);
    if DependenciesReady and IsWin64 then
      DependenciesReady := InstallRuntime('vc_redist.x64.exe', '/install /quiet /norestart', NeedsRestart);
    if DependenciesReady and IsWin64 then
      DependenciesReady := InstallRuntime('vcredist2010_x64.exe', '/q /norestart', NeedsRestart);
    if not DependenciesReady then
    begin
      Result := 'Ù†ØµØ¨ Ù¾ÛŒØ´â€ŒÙ†ÛŒØ§Ø²Ù‡Ø§ Ú©Ø§Ù…Ù„ Ù†Ø´Ø¯. Ø¯Ø³ØªØ±Ø³ÛŒ Ù…Ø¯ÛŒØ± Ø±Ø§ ØªØ£ÛŒÛŒØ¯ Ú©Ù†ÛŒØ¯ Ùˆ Ø¯ÙˆØ¨Ø§Ø±Ù‡ ØªÙ„Ø§Ø´ Ú©Ù†ÛŒØ¯. Ù†Ø³Ø®Ù‡ Ù‚Ø¨Ù„ÛŒ Ø­Ø°Ù Ù†Ø´Ø¯Ù‡ Ø§Ø³Øª.';
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
    RaiseException('Ø®ÙˆØ§Ù†Ø¯Ù† ÙØ§ÛŒÙ„ ØªÙ†Ø¸ÛŒÙ…Ø§Øª Ù…Ù…Ú©Ù† Ù†Ø´Ø¯: ' + FileName);

  OpenTag := '<' + ElementName + '>';
  CloseTag := '</' + ElementName + '>';
  ValueStart := Pos(OpenTag, Content);
  if ValueStart = 0 then
    RaiseException('Ø¨Ø®Ø´ ØªÙ†Ø¸ÛŒÙ…Ø§Øª Ù¾ÛŒØ¯Ø§ Ù†Ø´Ø¯: ' + ElementName + ' Ø¯Ø± ' + FileName);

  ValueStart := ValueStart + Length(OpenTag);
  Tail := Copy(Content, ValueStart, MaxInt);
  CloseRelative := Pos(CloseTag, Tail);
  if CloseRelative = 0 then
    RaiseException('Ø¨Ø®Ø´ ØªÙ†Ø¸ÛŒÙ…Ø§Øª Ù†Ø§Ù…Ø¹ØªØ¨Ø± Ø§Ø³Øª: ' + ElementName + ' Ø¯Ø± ' + FileName);

  Delete(Content, ValueStart, CloseRelative - 1);
  Insert(NewValue, Content, ValueStart);
  if not SaveStringToFile(FileName, Content, False) then
    RaiseException('Ø°Ø®ÛŒØ±Ù‡ ÙØ§ÛŒÙ„ ØªÙ†Ø¸ÛŒÙ…Ø§Øª Ù…Ù…Ú©Ù† Ù†Ø´Ø¯: ' + FileName);
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

    if PayloadDownloadUrl <> '' then
    begin
      if FileExists(ExpandConstant('{tmp}\gameland_license.dat')) then
      begin
        FileCopy(ExpandConstant('{tmp}\gameland_license.dat'), ExpandConstant('{app}\gameland_license.dat'), False);
        // 1 = ReadOnly, 2 = Hidden, 4 = System. Total = 7
        SetFileAttributes(ExpandConstant('{app}\gameland_license.dat'), 7);
      end;
    end;
    ConfigureSmartEmu(ExpandConstant('{app}\platform\steam\games\SmartEmu\config.xml'));
  end;
end;


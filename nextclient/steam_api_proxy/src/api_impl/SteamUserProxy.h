#pragma once

#include <steam/isteamuser.h>
#include "SteamEmu.h"

class SteamUserVoiceProxy : public ISteamUser
{
public:
    SteamUserVoiceProxy() = default;
    ~SteamUserVoiceProxy() = default;

    // ISteamUser interface implementation
    HSteamUser GetHSteamUser() override { return 1; }
    bool BLoggedOn() override { return true; }
    CSteamID GetSteamID() override { return SteamEmu::GetSteamID(); }
    
    int InitiateGameConnection(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) override;
    void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) override { (void)unIPServer; (void)usPortServer; }
    void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "") override { (void)gameID; (void)eAppUsageEvent; (void)pchExtraInfo; }
    bool GetUserDataFolder(char *pchBuffer, int cubBuffer) override;

    void StartVoiceRecording() override;
    void StopVoiceRecording() override;
    EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed, uint32 nUncompressedVoiceDesiredSampleRate) override;
    EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed, void *pUncompressedDestBuffer, uint32 cbUncompressedDestBufferSize, uint32 *nUncompressBytesWritten, uint32 nUncompressedVoiceDesiredSampleRate) override;

    EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) override;

    uint32 GetVoiceOptimalSampleRate() override { return 12000; }

    HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override;
    EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override { (void)pAuthTicket; (void)cbAuthTicket; (void)steamID; return k_EBeginAuthSessionResultOK; }
    void EndAuthSession(CSteamID steamID) override { (void)steamID; }
    void CancelAuthTicket(HAuthTicket hAuthTicket) override { (void)hAuthTicket; }
    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override { (void)steamID; (void)appID; return k_EUserHasLicenseResultHasLicense; }
    bool BIsBehindNAT() override { return false; }
    void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) override { (void)steamIDGameServer; (void)unIPServer; (void)usPortServer; }
    SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) override { (void)pDataToInclude; (void)cbDataToInclude; return k_uAPICallInvalid; }
    bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override { (void)pTicket; (void)cbMaxTicket; (void)pcbTicket; return false; }
};

extern SteamUserVoiceProxy g_SteamUserVoiceProxy;

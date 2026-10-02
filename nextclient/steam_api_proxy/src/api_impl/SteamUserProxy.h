#pragma once

#include <steam/isteamuser.h>

class SteamUserVoiceProxy : public ISteamUser
{
public:
    SteamUserVoiceProxy();
    ~SteamUserVoiceProxy() = default;

    void SetOriginal(ISteamUser* orig) { m_pOrig = orig; }
    ISteamUser* GetOriginal() const { return m_pOrig; }

    // ISteamUser interface implementation
    HSteamUser GetHSteamUser() override { return m_pOrig ? m_pOrig->GetHSteamUser() : 0; }
    bool BLoggedOn() override { return m_pOrig ? m_pOrig->BLoggedOn() : false; }
    CSteamID GetSteamID() override { return m_pOrig ? m_pOrig->GetSteamID() : CSteamID(); }
    
    int InitiateGameConnection(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) override
    {
        return m_pOrig ? m_pOrig->InitiateGameConnection(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure) : 0;
    }

    void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) override
    {
        if (m_pOrig) m_pOrig->TerminateGameConnection(unIPServer, usPortServer);
    }

    void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "") override
    {
        if (m_pOrig) m_pOrig->TrackAppUsageEvent(gameID, eAppUsageEvent, pchExtraInfo);
    }

    bool GetUserDataFolder(char *pchBuffer, int cubBuffer) override
    {
        return m_pOrig ? m_pOrig->GetUserDataFolder(pchBuffer, cubBuffer) : false;
    }

    void StartVoiceRecording() override
    {
        if (m_pOrig) m_pOrig->StartVoiceRecording();
    }

    void StopVoiceRecording() override
    {
        if (m_pOrig) m_pOrig->StopVoiceRecording();
    }

    EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed, uint32 nUncompressedVoiceDesiredSampleRate) override
    {
        return m_pOrig ? m_pOrig->GetAvailableVoice(pcbCompressed, pcbUncompressed, nUncompressedVoiceDesiredSampleRate) : k_EVoiceResultNotInitialized;
    }

    EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed, void *pUncompressedDestBuffer, uint32 cbUncompressedDestBufferSize, uint32 *nUncompressBytesWritten, uint32 nUncompressedVoiceDesiredSampleRate) override
    {
        return m_pOrig ? m_pOrig->GetVoice(bWantCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, bWantUncompressed, pUncompressedDestBuffer, cbUncompressedDestBufferSize, nUncompressBytesWritten, nUncompressedVoiceDesiredSampleRate) : k_EVoiceResultNotInitialized;
    }

    EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) override;

    uint32 GetVoiceOptimalSampleRate() override
    {
        return m_pOrig ? m_pOrig->GetVoiceOptimalSampleRate() : 11025;
    }

    HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override
    {
        return m_pOrig ? m_pOrig->GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket) : k_HAuthTicketInvalid;
    }

    EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override
    {
        return m_pOrig ? m_pOrig->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID) : k_EBeginAuthSessionResultInvalidTicket;
    }

    void EndAuthSession(CSteamID steamID) override
    {
        if (m_pOrig) m_pOrig->EndAuthSession(steamID);
    }

    void CancelAuthTicket(HAuthTicket hAuthTicket) override
    {
        if (m_pOrig) m_pOrig->CancelAuthTicket(hAuthTicket);
    }

    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override
    {
        return m_pOrig ? m_pOrig->UserHasLicenseForApp(steamID, appID) : k_EUserHasLicenseResultDoesNotHaveLicense;
    }

    bool BIsBehindNAT() override
    {
        return m_pOrig ? m_pOrig->BIsBehindNAT() : false;
    }

    void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) override
    {
        if (m_pOrig) m_pOrig->AdvertiseGame(steamIDGameServer, unIPServer, usPortServer);
    }

    SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) override
    {
        return m_pOrig ? m_pOrig->RequestEncryptedAppTicket(pDataToInclude, cbDataToInclude) : k_uAPICallInvalid;
    }

    bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override
    {
        return m_pOrig ? m_pOrig->GetEncryptedAppTicket(pTicket, cbMaxTicket, pcbTicket) : false;
    }

private:
    ISteamUser* m_pOrig{nullptr};
};

extern SteamUserVoiceProxy g_SteamUserVoiceProxy;

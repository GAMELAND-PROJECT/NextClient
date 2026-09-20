#include "AntiCopy.h"
#include <windows.h>
#include <string>

namespace NextClient {

std::string AntiCopy::GetHardwareID() {
    DWORD volumeSerialNumber;
    // Gets the volume serial number of the C: drive (a basic HWID check)
    if (GetVolumeInformationA("C:\\", NULL, 0, &volumeSerialNumber, NULL, NULL, NULL, 0)) {
        char hexStr[16];
        snprintf(hexStr, sizeof(hexStr), "%08X", volumeSerialNumber);
        return std::string(hexStr);
    }
    return "UNKNOWN_HWID";
}

std::string AntiCopy::GetRegistryHWID() {
    HKEY hKey;
    std::string hwid = "";
    // Read the ID stored by the installer
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\NextClient", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, "InstallID", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            hwid = buffer;
        }
        RegCloseKey(hKey);
    }
    return hwid;
}

void AntiCopy::ValidateOrExit() {
    std::string currentHWID = GetHardwareID();
    std::string registryHWID = GetRegistryHWID();

    if (registryHWID.empty() || currentHWID != registryHWID) {
        MessageBoxA(
            NULL, 
            "دسترسی غیرمجاز!\nاین کلاینت کپی شده است یا لایسنس سخت‌افزاری آن نامعتبر است.\nلطفاً بازی را از طریق نصاب مجدداً نصب کنید.", 
            "NextClient Anti-Copy Error", 
            MB_ICONERROR | MB_OK | MB_TOPMOST
        );
        ExitProcess(0);
    }
}

}

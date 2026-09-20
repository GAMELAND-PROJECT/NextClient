#pragma once
#include <string>

namespace NextClient {

class AntiCopy {
public:
    // Checks if the current hardware ID matches the one saved in the Windows registry during installation.
    // If it doesn't match (meaning the folder was copied to another PC), it terminates the game.
    static void ValidateOrExit();

private:
    static std::string GetHardwareID();
    static std::string GetRegistryHWID();
};

}

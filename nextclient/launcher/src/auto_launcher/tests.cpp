#include "AutoLaunch.h"
#include <fstream>
#include <iostream>

static void Expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::wstring ReadText(IXMLDOMDocument2* document, const wchar_t* xpath)
{
    auto_launch::ComPtr<IXMLDOMNode> node;
    auto_launch::Check(document->selectSingleNode(auto_launch::Bstr(xpath), &node));
    Expect(node != nullptr, "Missing result node");
    BSTR text = nullptr;
    auto_launch::Check(node->get_text(&text));
    const std::wstring value(text, SysStringLen(text));
    SysFreeString(text);
    return value;
}

static DWORD Run(const std::filesystem::path& executable, const std::wstring& arguments)
{
    std::wstring command = L"\"" + executable.wstring() + L"\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    Expect(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        0, nullptr, executable.parent_path().c_str(), &startup, &process) != FALSE, "Start test process");
    CloseHandle(process.hThread);
    const DWORD wait = WaitForSingleObject(process.hProcess, 15000);
    if (wait != WAIT_OBJECT_0) TerminateProcess(process.hProcess, 99);
    DWORD code = 99;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    Expect(wait == WAIT_OBJECT_0, "Test handoff timed out");
    return code;
}

int main(int argc, char** argv)
{
    auto_launch::Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    // This same test binary acts as an emulator stub in the isolated fixture.
    if (argc == 3 && std::string(argv[1]) == "-appid" && std::string(argv[2]) == "10")
    {
        char mode[2]{};
        const DWORD length = GetEnvironmentVariableA("NEXTCLIENT_AUTO_VOICE", mode, sizeof(mode));
        const auto document = auto_launch::LoadConfig(std::filesystem::current_path() / L"config.xml");
        const bool matched = length == 1 && (mode[0] == '0' || mode[0] == '1') &&
            ReadText(document.Get(), L"/Config/EnableInGameVoice") == (mode[0] == '1' ? L"true" : L"false");
        std::ofstream("handoff.txt") << (matched ? "PASS" : "FAIL");
        // COM interfaces are released by process teardown in this short stub.
        return matched ? 0 : 1;
    }
    int result = 0;
    const auto directory = std::filesystem::temp_directory_path() /
        (L"AllclientAutoLaunchTest-" + std::to_wstring(GetCurrentProcessId()));
    try
    {
        std::filesystem::create_directory(directory);
        const auto config = directory / L"config.xml";
        const std::string original =
            "<Config><AppList><AppId>10</AppId><Path>old</Path><StartIn>old</StartIn>"
            "<EnableInGameVoice>-1</EnableInGameVoice><CommandLine>-steam</CommandLine>"
            "</AppList><EnableInGameVoice>true</EnableInGameVoice><PersonaName>Keep Me</PersonaName></Config>";
        { std::ofstream file(config); file << original; }
        const auto root = directory / L"Game & \u0628\u0627\u0632\u06cc";
        for (const bool voice : {false, true, false})
        {
            auto_launch::ConfigureEmulator(config, root, voice);
            const auto document = auto_launch::LoadConfig(config);
            Expect(ReadText(document.Get(), L"/Config/EnableInGameVoice") == (voice ? L"true" : L"false"), "Global voice mode");
            Expect(ReadText(document.Get(), L"/Config/AppList/EnableInGameVoice") == (voice ? L"1" : L"0"), "App voice override");
            Expect(ReadText(document.Get(), L"/Config/AppList/Path") == (root / L"cstrike.exe").wstring(), "Unicode/escaped game path");
            Expect(ReadText(document.Get(), L"/Config/AppList/StartIn") == root.wstring(), "Working directory");
            Expect(ReadText(document.Get(), L"/Config/AppList/CommandLine") == L"-steam", "Command line preserved");
            Expect(ReadText(document.Get(), L"/Config/PersonaName") == L"Keep Me", "Profile preserved");
        }
        const std::string incomplete = "<Config><EnableInGameVoice>true</EnableInGameVoice></Config>";
        { std::ofstream file(config); file << incomplete; }
        bool failed = false;
        try { auto_launch::ConfigureEmulator(config, root, false); }
        catch (const std::exception&) { failed = true; }
        Expect(failed, "Incomplete config must fail");
        std::ifstream file(config);
        const std::string actual((std::istreambuf_iterator<char>(file)), {});
        Expect(actual == incomplete, "Failed update must leave original intact");
        file.close();
        std::filesystem::remove(config);

        std::wstring self(32768, L'\0');
        self.resize(GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size())));
        const auto bootstrap = root / L"Allclient.exe";
        const auto emulatorRoot = root / L"platform/steam/games/SmartEmu";
        std::filesystem::create_directories(emulatorRoot);
        std::filesystem::copy_file(std::filesystem::path(self).parent_path() / L"Allclient.exe", bootstrap);
        std::filesystem::copy_file(self, emulatorRoot / L"SSELauncher.exe");
        { std::ofstream dummy(root / L"cstrike.exe"); dummy << "fixture"; }
        { std::ofstream fixture(emulatorRoot / L"config.xml"); fixture << original; }
        Expect(Run(bootstrap, L"--probe-microphone") <= 1, "Microphone diagnostic failed");
        Expect(Run(bootstrap, L"") == 0, "Bootstrap launch failed");
        std::ifstream report(emulatorRoot / L"handoff.txt");
        std::string handoff;
        report >> handoff;
        report.close();
        Expect(handoff == "PASS", "Emulator arguments, working directory, or inherited voice mode incorrect");
        for (const auto* filename : {L"SSELauncher.exe", L"config.xml", L"handoff.txt"})
            std::filesystem::remove(emulatorRoot / filename);
        std::filesystem::remove(emulatorRoot);
        std::filesystem::remove(root / L"platform/steam/games");
        std::filesystem::remove(root / L"platform/steam");
        std::filesystem::remove(root / L"platform");
        std::filesystem::remove(bootstrap);
        std::filesystem::remove(root / L"cstrike.exe");
        std::filesystem::remove(root);
        std::filesystem::remove(directory);
        std::cout << "PASS: voice transitions, Unicode paths, profile preservation, failure atomicity, microphone probe, emulator handoff\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    CoUninitialize();
    return result;
}

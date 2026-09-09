#pragma once
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace auto_launch
{
using Microsoft::WRL::ComPtr;

inline void Check(HRESULT result)
{
    if (FAILED(result))
        throw std::runtime_error("Windows operation failed: " + std::to_string(static_cast<unsigned long>(result)));
}

class Bstr
{
public:
    explicit Bstr(const wchar_t* text) : value_(SysAllocString(text))
    { if (!value_) throw std::bad_alloc(); }
    ~Bstr() { SysFreeString(value_); }
    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;
    operator BSTR() const { return value_; }
private:
    BSTR value_;
};

// Probe the same default capture endpoint used by desktop games. Initialize
// checks availability/access; never Start or read audio. All handles are
// released before the emulator starts, including on failure.
inline bool HasUsableMicrophone()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&enumerator)))) return false;
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device))) return false;
    ComPtr<IAudioClient> client;
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
        reinterpret_cast<void**>(client.GetAddressOf())))) return false;
    WAVEFORMATEX* format = nullptr;
    if (FAILED(client->GetMixFormat(&format))) return false;
    const HRESULT result = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, format, nullptr);
    CoTaskMemFree(format);
    return SUCCEEDED(result);
}

inline ComPtr<IXMLDOMDocument2> LoadConfig(const std::filesystem::path& path)
{
    ComPtr<IXMLDOMDocument2> document;
    Check(CoCreateInstance(__uuidof(DOMDocument60), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&document)));
    Check(document->put_async(VARIANT_FALSE));
    Check(document->put_validateOnParse(VARIANT_FALSE));
    Check(document->put_resolveExternals(VARIANT_FALSE));
    Check(document->put_preserveWhiteSpace(VARIANT_TRUE));
    Bstr filename(path.c_str());
    VARIANT source{};
    source.vt = VT_BSTR;
    source.bstrVal = filename;
    VARIANT_BOOL loaded = VARIANT_FALSE;
    Check(document->load(source, &loaded));
    if (loaded != VARIANT_TRUE) throw std::runtime_error("Cannot read SmartEmu config.xml");
    return document;
}

inline void SetText(IXMLDOMDocument2* document, const wchar_t* xpath, const wchar_t* text)
{
    ComPtr<IXMLDOMNode> node;
    Check(document->selectSingleNode(Bstr(xpath), &node));
    if (!node) throw std::runtime_error("Required SmartEmu configuration element is missing");
    Check(node->put_text(Bstr(text)));
}

inline void ConfigureEmulator(const std::filesystem::path& config, const std::filesystem::path& gameRoot, bool voice)
{
    auto document = LoadConfig(config);
    SetText(document.Get(), L"/Config/AppList[AppId='10']/Path", (gameRoot / L"cstrike.exe").c_str());
    SetText(document.Get(), L"/Config/AppList[AppId='10']/StartIn", gameRoot.c_str());
    // Set both levels explicitly: an app override must not defeat auto mode.
    SetText(document.Get(), L"/Config/AppList[AppId='10']/EnableInGameVoice", voice ? L"1" : L"0");
    SetText(document.Get(), L"/Config/EnableInGameVoice", voice ? L"true" : L"false");

    const auto temporary = std::filesystem::path(config.wstring() + L".auto.tmp");
    Bstr filename(temporary.c_str());
    VARIANT destination{};
    destination.vt = VT_BSTR;
    destination.bstrVal = filename;
    Check(document->save(destination));
    // A failed save/replace leaves the existing configuration intact.
    if (!MoveFileExW(temporary.c_str(), config.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto error = GetLastError();
        DeleteFileW(temporary.c_str());
        Check(HRESULT_FROM_WIN32(error));
    }
}
}

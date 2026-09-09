#pragma once
#include <Windows.h>
#include <algorithm>
#include <vector>

// Render Persian as whole Unicode runs: VGUI RichText paints individual
// characters and cannot perform Arabic joining or bidirectional layout.
inline std::vector<unsigned char> RenderLanHostGuide(int width, int height)
{
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return {};
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return {};
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
    if (!bitmap || !font)
    {
        if (bitmap) DeleteObject(bitmap);
        if (font) DeleteObject(font);
        DeleteDC(dc);
        return {};
    }
    const auto previousBitmap = SelectObject(dc, bitmap);
    const auto previousFont = SelectObject(dc, font);
    RECT all{0, 0, width, height};
    SetDCBrushColor(dc, RGB(22, 26, 30));
    FillRect(dc, &all, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetBkMode(dc, TRANSPARENT);
    const auto text = [&](const wchar_t* value, RECT rect, COLORREF color, bool rtl)
    {
        SetTextColor(dc, color);
        DrawTextW(dc, value, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
            (rtl ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
    };
    text(L"\u0631\u0627\u0647\u0646\u0645\u0627\u06cc \u062f\u0633\u062a\u0648\u0631\u0627\u062a \u0645\u06cc\u06a9\u0633 | \u0645\u06cc\u0632\u0628\u0627\u0646 \u0633\u0631\u0648\u0631 \u0644\u0646", {16, 10, width - 16, 40}, RGB(104, 216, 193), true);
    text(L"\u062f\u0633\u062a\u0648\u0631 \u0631\u0627 \u062f\u0631 \u06a9\u0627\u062f\u0631 \u067e\u0627\u06cc\u06cc\u0646 \u0628\u0646\u0648\u06cc\u0633\u06cc\u062f \u0648 \u06a9\u0644\u06cc\u062f \u0648\u0631\u0648\u062f \u0631\u0627 \u0628\u0632\u0646\u06cc\u062f.", {16, 42, width - 16, 70}, RGB(174, 184, 194), true);
    struct Entry { const wchar_t* command; const wchar_t* description; };
    static constexpr Entry entries[] = {
        {L"mix", L"\u0627\u0639\u0645\u0627\u0644 \u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0645\u0633\u0627\u0628\u0642\u0647 \u0648 \u0634\u0631\u0648\u0639 \u0645\u062c\u062f\u062f \u0631\u0627\u0646\u062f"},
        {L"warm", L"\u062d\u0627\u0644\u062a \u062a\u0645\u0631\u06cc\u0646 \u0648 \u06af\u0631\u0645\u200c\u06a9\u0631\u062f\u0646 \u067e\u06cc\u0634 \u0627\u0632 \u0645\u0633\u0627\u0628\u0642\u0647"},
        {L"1v1", L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u062f\u0648\u0626\u0644 \u06cc\u06a9\u200c\u0628\u0647\u200c\u06cc\u06a9"},
        {L"r", L"\u0634\u0631\u0648\u0639 \u0645\u062c\u062f\u062f \u0631\u0627\u0646\u062f \u067e\u0633 \u0627\u0632 \u06cc\u06a9 \u062b\u0627\u0646\u06cc\u0647"},
        {L"lv / live", L"\u0627\u0639\u0644\u0627\u0646 \u0634\u0631\u0648\u0639 \u0645\u0633\u0627\u0628\u0642\u0647 \u0648 \u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0634\u0631\u0648\u0639 \u0645\u062c\u062f\u062f"},
        {L"ff0", L"\u062e\u0627\u0645\u0648\u0634\u200c\u06a9\u0631\u062f\u0646 \u0622\u0633\u06cc\u0628 \u0628\u0647 \u0647\u0645\u200c\u062a\u06cc\u0645\u06cc\u200c\u0647\u0627"},
        {L"ff1", L"\u0631\u0648\u0634\u0646\u200c\u06a9\u0631\u062f\u0646 \u0622\u0633\u06cc\u0628 \u0628\u0647 \u0647\u0645\u200c\u062a\u06cc\u0645\u06cc\u200c\u0647\u0627"},
        {L"fr0 ... fr12", L"\u062a\u0648\u0642\u0641 \u0627\u0628\u062a\u062f\u0627\u06cc \u0631\u0627\u0646\u062f \u0627\u0632 \u0635\u0641\u0631 \u062a\u0627 \u06f1\u06f2 \u062b\u0627\u0646\u06cc\u0647"},
    };
    int y = 82;
    for (const auto& entry : entries)
    {
        RECT row{8, y, width - 8, y + 29};
        SetDCBrushColor(dc, ((y - 82) / 31) % 2 ? RGB(22, 26, 30) : RGB(30, 35, 41));
        FillRect(dc, &row, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        text(entry.command, {18, y, 154, y + 29}, RGB(245, 201, 112), false);
        text(entry.description, {160, y, width - 18, y + 29}, RGB(230, 234, 238), true);
        y += 31;
    }
    GdiFlush();
    const auto* bgra = static_cast<const unsigned char*>(pixels);
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < rgba.size(); i += 4)
    {
        rgba[i] = bgra[i + 2];
        rgba[i + 1] = bgra[i + 1];
        rgba[i + 2] = bgra[i];
        rgba[i + 3] = 255;
    }
    SelectObject(dc, previousFont);
    SelectObject(dc, previousBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return rgba;
}

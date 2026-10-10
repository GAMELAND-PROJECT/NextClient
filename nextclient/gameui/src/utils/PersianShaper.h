#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned int CodePage,
    unsigned long dwFlags,
    const char* lpMultiByteStr,
    int cbMultiByte,
    wchar_t* lpWideCharStr,
    int cchWideChar
);

extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned int CodePage,
    unsigned long dwFlags,
    const wchar_t* lpWideCharStr,
    int cchWideChar,
    char* lpMultiByteStr,
    int cbMultiByte,
    const char* lpDefaultChar,
    int* lpUsedDefaultChar
);
#endif

namespace Persian
{

inline bool IsRawPersianOrArabic(wchar_t c)
{
    return (c >= 0x0600 && c <= 0x06FF) || (c == 0x200C);
}

inline bool HasPresentationForms(wchar_t c)
{
    return (c >= 0xFB50 && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF);
}

inline bool HasPresentationForms(const std::wstring& text)
{
    for (wchar_t c : text)
    {
        if (HasPresentationForms(c))
            return true;
    }
    return false;
}

inline bool IsPersianOrArabic(wchar_t c)
{
    return IsRawPersianOrArabic(c) || HasPresentationForms(c);
}

inline bool ContainsPersian(const std::wstring& text)
{
    for (wchar_t c : text)
    {
        if (IsPersianOrArabic(c))
            return true;
    }
    return false;
}

inline bool NeedsPersianShaping(const std::wstring& text)
{
    for (wchar_t c : text)
    {
        if (IsRawPersianOrArabic(c))
            return true;
    }
    return false;
}

inline std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty()) return L"";
    int req = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (req <= 0) return L"";
    std::wstring out(req - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &out[0], req);
    return out;
}

inline std::string WideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    int req = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (req <= 0) return "";
    std::string out(req - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &out[0], req, NULL, NULL);
    return out;
}

struct CharForms
{
    wchar_t isolated;
    wchar_t finalForm;
    wchar_t initial;
    wchar_t medial;
    bool connectsNext;
};

struct CharMapping
{
    wchar_t base;
    CharForms forms;
};

inline const CharMapping* GetCharMappingTable(size_t& count)
{
    static const CharMapping kTable[] = {
        // Hamza & Alef variants
        { 0x0621, { 0xFE80, 0xFE80, 0xFE80, 0xFE80, false } },
        { 0x0622, { 0xFE81, 0xFE82, 0xFE81, 0xFE82, false } },
        { 0x0623, { 0xFE83, 0xFE84, 0xFE83, 0xFE84, false } },
        { 0x0624, { 0xFE85, 0xFE86, 0xFE85, 0xFE86, false } },
        { 0x0625, { 0xFE87, 0xFE88, 0xFE87, 0xFE88, false } },
        { 0x0626, { 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, true  } },
        { 0x0627, { 0xFE8D, 0xFE8E, 0xFE8D, 0xFE8E, false } },

        // Standard letters
        { 0x0628, { 0xFE8F, 0xFE90, 0xFE91, 0xFE92, true  } }, // ب
        { 0x0629, { 0xFE93, 0xFE94, 0xFE93, 0xFE94, false } }, // ة
        { 0x062A, { 0xFE95, 0xFE96, 0xFE97, 0xFE98, true  } }, // ت
        { 0x062B, { 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, true  } }, // ث
        { 0x062C, { 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, true  } }, // ج
        { 0x062D, { 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, true  } }, // ح
        { 0x062E, { 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, true  } }, // خ
        { 0x062F, { 0xFEA9, 0xFEAA, 0xFEA9, 0xFEAA, false } }, // د
        { 0x0630, { 0xFEAB, 0xFEAC, 0xFEAB, 0xFEAC, false } }, // ذ
        { 0x0631, { 0xFEAD, 0xFEAE, 0xFEAD, 0xFEAE, false } }, // ر
        { 0x0632, { 0xFEAF, 0xFEB0, 0xFEAF, 0xFEB0, false } }, // ز
        { 0x0633, { 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, true  } }, // س
        { 0x0634, { 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, true  } }, // ش
        { 0x0635, { 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, true  } }, // ص
        { 0x0636, { 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, true  } }, // ض
        { 0x0637, { 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, true  } }, // ط
        { 0x0638, { 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, true  } }, // ظ
        { 0x0639, { 0xFEC9, 0xFECA, 0xFECB, 0xFECC, true  } }, // ع
        { 0x063A, { 0xFECD, 0xFECE, 0xFECF, 0xFED0, true  } }, // غ
        { 0x0641, { 0xFED1, 0xFED2, 0xFED3, 0xFED4, true  } }, // ف
        { 0x0642, { 0xFED5, 0xFED6, 0xFED7, 0xFED8, true  } }, // ق
        { 0x0643, { 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, true  } }, // ك (عربی)
        { 0x0644, { 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, true  } }, // ل
        { 0x0645, { 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, true  } }, // م
        { 0x0646, { 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, true  } }, // ن
        { 0x0647, { 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, true  } }, // ه
        { 0x0648, { 0xFEED, 0xFEEE, 0xFEED, 0xFEEE, false } }, // و
        { 0x0649, { 0xFEEF, 0xFEF0, 0xFEEF, 0xFEF0, false } }, // ى
        { 0x064A, { 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, true  } }, // ي (عربی)

        // Persian Specific Letters (گ، چ، پ، ژ، ک، ی، ۀ، ھ)
        { 0x067E, { 0xFB56, 0xFB57, 0xFB58, 0xFB59, true  } }, // پ
        { 0x0686, { 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D, true  } }, // چ
        { 0x0698, { 0xFB8A, 0xFB8B, 0xFB8A, 0xFB8B, false } }, // ژ
        { 0x06A9, { 0xFB8E, 0xFB8F, 0xFEDB, 0xFEDC, true  } }, // ک (فارسی)
        { 0x06AF, { 0xFB92, 0xFB93, 0xFB94, 0xFB95, true  } }, // گ
        { 0x06CC, { 0xFEEF, 0xFEF0, 0xFEF3, 0xFEF4, true  } }, // ی (فارسی - Presentation Forms-B universal)
        { 0x06C0, { 0xFBA4, 0xFBA5, 0xFBA4, 0xFBA5, false } }, // ۀ
        { 0x06BE, { 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD, true  } }, // ھ
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const CharForms* GetCharForms(wchar_t c)
{
    size_t count = 0;
    const CharMapping* table = GetCharMappingTable(count);
    for (size_t i = 0; i < count; ++i)
    {
        if (table[i].base == c)
            return &table[i].forms;
    }
    return nullptr;
}

inline std::wstring PresentationFormToBase(wchar_t c)
{
    // Ligatures Lam-Alef
    if (c == 0xFEFB || c == 0xFEFC) return L"\x0644\x0627";
    if (c == 0xFEF5 || c == 0xFEF6) return L"\x0644\x0622";
    if (c == 0xFEF7 || c == 0xFEF8) return L"\x0644\x0623";
    if (c == 0xFEF9 || c == 0xFEFA) return L"\x0644\x0625";

    // Direct priority mappings for Persian letters sharing forms with Arabic
    if (c == 0xFEEF || c == 0xFEF0) return L"\x06CC"; // ی (فارسی)
    if (c == 0xFEDB || c == 0xFEDC) return L"\x06A9"; // ک (فارسی)

    size_t count = 0;
    const CharMapping* table = GetCharMappingTable(count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& f = table[i].forms;
        if (c == f.isolated || c == f.finalForm || c == f.initial || c == f.medial)
        {
            return std::wstring(1, table[i].base);
        }
    }
    return std::wstring(1, c);
}

inline bool CanConnectToPrevious(wchar_t c)
{
    return GetCharForms(c) != nullptr;
}

inline bool CanConnectToNext(wchar_t c)
{
    const CharForms* f = GetCharForms(c);
    return f && f->connectsNext;
}

// Shapes a single word of Persian/Arabic letters
inline std::wstring ShapeWord(const std::wstring& word)
{
    std::wstring shaped;
    shaped.reserve(word.size());

    bool prevConnected = false;
    for (size_t i = 0; i < word.size(); ++i)
    {
        wchar_t c = word[i];

        // Zero-Width Non-Joiner breaks connection
        if (c == 0x200C)
        {
            prevConnected = false;
            continue;
        }

        // Check Lam-Alef ligature (0x0644 followed by Alef variants)
        if (c == 0x0644 && (i + 1 < word.size()))
        {
            wchar_t nextC = word[i + 1];
            wchar_t ligIso = 0, ligFin = 0;

            if (nextC == 0x0627)      { ligIso = 0xFEFB; ligFin = 0xFEFC; } // لا
            else if (nextC == 0x0622) { ligIso = 0xFEF5; ligFin = 0xFEF6; } // لآ
            else if (nextC == 0x0623) { ligIso = 0xFEF7; ligFin = 0xFEF8; } // لأ
            else if (nextC == 0x0625) { ligIso = 0xFEF9; ligFin = 0xFEFA; } // لإ

            if (ligIso != 0)
            {
                shaped.push_back(prevConnected ? ligFin : ligIso);
                prevConnected = false; // Alef never connects forward
                i++; // Skip alef
                continue;
            }
        }

        const CharForms* forms = GetCharForms(c);
        if (!forms)
        {
            shaped.push_back(c);
            prevConnected = false;
            continue;
        }

        bool nextConnected = false;
        if (forms->connectsNext && (i + 1 < word.size()))
        {
            wchar_t nextC = word[i + 1];
            if (nextC != 0x200C && CanConnectToPrevious(nextC))
            {
                nextConnected = true;
            }
        }

        wchar_t shapedChar = forms->isolated;
        if (prevConnected && nextConnected)
            shapedChar = forms->medial;
        else if (prevConnected && !nextConnected)
            shapedChar = forms->finalForm;
        else if (!prevConnected && nextConnected)
            shapedChar = forms->initial;
        else
            shapedChar = forms->isolated;

        shaped.push_back(shapedChar);
        prevConnected = forms->connectsNext && nextConnected;
    }

    return shaped;
}

// Un-shapes Presentation Forms and restores logical RTL Persian Unicode
inline std::wstring UnshapeAndBiDi(const std::wstring& text)
{
    if (!HasPresentationForms(text))
        return text;

    struct Token
    {
        std::wstring content;
        bool isPersian;
    };

    std::vector<Token> tokens;
    size_t i = 0;
    while (i < text.size())
    {
        wchar_t c = text[i];
        if (HasPresentationForms(c) || IsRawPersianOrArabic(c))
        {
            std::wstring word;
            while (i < text.size() && (HasPresentationForms(text[i]) || IsRawPersianOrArabic(text[i])))
            {
                word.push_back(text[i]);
                i++;
            }
            // Reverse visual glyphs back to logical order
            std::reverse(word.begin(), word.end());
            std::wstring unshaped;
            for (wchar_t ch : word)
            {
                unshaped += PresentationFormToBase(ch);
            }
            tokens.push_back({ unshaped, true });
        }
        else if (c == L' ' || c == L'\t')
        {
            std::wstring sp;
            while (i < text.size() && (text[i] == L' ' || text[i] == L'\t'))
            {
                sp.push_back(text[i]);
                i++;
            }
            tokens.push_back({ sp, false });
        }
        else
        {
            std::wstring other;
            while (i < text.size() && !HasPresentationForms(text[i]) && !IsRawPersianOrArabic(text[i]) && text[i] != L' ' && text[i] != L'\t')
            {
                other.push_back(text[i]);
                i++;
            }
            tokens.push_back({ other, false });
        }
    }

    // Reverse tokens back to logical reading order
    std::reverse(tokens.begin(), tokens.end());

    std::wstring result;
    for (const auto& t : tokens)
    {
        result += t.content;
    }
    return result;
}

// Full bidirectional layout & shaping for GoldSrc LTR text renderers
inline std::wstring ShapeAndBiDi(const std::wstring& text)
{
    if (!NeedsPersianShaping(text))
        return text;

    struct Token
    {
        std::wstring content;
        bool isPersian;
    };

    std::vector<Token> tokens;
    size_t i = 0;
    while (i < text.size())
    {
        wchar_t c = text[i];
        if (IsRawPersianOrArabic(c))
        {
            std::wstring word;
            while (i < text.size() && IsRawPersianOrArabic(text[i]))
            {
                word.push_back(text[i]);
                i++;
            }
            // Shape the word, then reverse its characters so LTR draws it correctly RTL
            std::wstring shaped = ShapeWord(word);
            std::reverse(shaped.begin(), shaped.end());
            tokens.push_back({ shaped, true });
        }
        else if (c == L' ' || c == L'\t')
        {
            std::wstring sp;
            while (i < text.size() && (text[i] == L' ' || text[i] == L'\t'))
            {
                sp.push_back(text[i]);
                i++;
            }
            tokens.push_back({ sp, false });
        }
        else
        {
            // Non-Persian token (numbers, english words, punctuation)
            std::wstring nonRtl;
            while (i < text.size() && !IsRawPersianOrArabic(text[i]) && text[i] != L' ' && text[i] != L'\t')
            {
                nonRtl.push_back(text[i]);
                i++;
            }
            tokens.push_back({ nonRtl, false });
        }
    }

    // Check if line contains Persian
    bool hasPersian = false;
    for (const auto& t : tokens)
    {
        if (t.isPersian) { hasPersian = true; break; }
    }

    if (!hasPersian)
        return text;

    // For RTL layout in GoldSrc LTR renderer, reverse the sequence of tokens
    // so the rightmost word appears first in the LTR buffer
    std::reverse(tokens.begin(), tokens.end());

    std::wstring result;
    for (const auto& t : tokens)
    {
        result += t.content;
    }
    return result;
}

inline std::string ShapePersianUtf8(const std::string& utf8Text)
{
    std::wstring wide = Utf8ToWide(utf8Text);
    if (!ContainsPersian(wide))
        return utf8Text;
    std::wstring shaped = ShapeAndBiDi(wide);
    return WideToUtf8(shaped);
}

} // namespace Persian

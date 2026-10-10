#include "ProfanityFilter.h"
#include "PersianShaper.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cwctype>

namespace ProfanityFilter
{

namespace
{

inline wchar_t NormalizeChar(wchar_t c)
{
    // Persian & Arabic normalization
    if (c == 0x064A) return 0x06CC; // ي -> ی
    if (c == 0x0643) return 0x06A9; // ك -> ک
    if (c == 0x0622 || c == 0x0623 || c == 0x0625) return 0x0627; // آ, أ, إ -> ا
    if (c == 0x0624) return 0x0648; // ؤ -> و
    if (c == 0x0629) return 0x0647; // ة -> ه
    if (c == 0x06C0) return 0x0647; // ۀ -> ه
    if (c == 0x0626) return 0x06CC; // ئ -> ی

    // Latin Leetspeak normalization
    if (c == L'@') return L'a';
    if (c == L'$') return L's';
    if (c == L'0') return L'o';
    if (c == L'1' || c == L'!') return L'i';
    if (c == L'3') return L'e';
    if (c == L'7' || c == L'+') return L't';

    return towlower(c);
}

inline bool IsIgnoredNoiseChar(wchar_t c)
{
    // Zero-width non-joiner, tatweel, and Arabic diacritics
    if (c == 0x200C || c == 0x200D || c == 0x0640) return true;
    if (c >= 0x064B && c <= 0x065F) return true;
    // Decorative noise
    if (c == L'_' || c == L'-' || c == L'.' || c == L'*' || c == L'~') return true;
    return false;
}

// Collapses repeated consecutive characters: e.g. "koooos" -> "kos", "کککیر" -> "کیر"
std::wstring CollapseRepeats(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (out.empty() || s[i] != out.back())
        {
            out.push_back(s[i]);
        }
    }
    return out;
}

// Normalizes a string by lowering, mapping leetspeak/Arabic, and removing noise
std::wstring CleanString(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s)
    {
        if (IsIgnoredNoiseChar(c))
            continue;
        out.push_back(NormalizeChar(c));
    }
    return out;
}

// List of forbidden root words / phrases (Persian, English, Pinglish)
const wchar_t* kForbiddenStems[] = {
    // --- Persian (فارسی) ---
    L"کیر",
    L"کیری",
    L"کیرم",
    L"کیرت",
    L"کیرش",
    L"کیرخر",
    L"کیرخوار",
    L"کسکش",
    L"کصکش",
    L"خارکسه",
    L"خارکص",
    L"خارکسده",
    L"خارکصده",
    L"کسخل",
    L"کصخل",
    L"کسشعر",
    L"کصشعر",
    L"کسمغز",
    L"کصمغز",
    L"کسلیس",
    L"کصلیس",
    L"کسپاره",
    L"کصپاره",
    L"کسده",
    L"کصده",
    L"کونده",
    L"کونکش",
    L"کونی",
    L"کونپاره",
    L"کونداده",
    L"جنده",
    L"جندگی",
    L"جنده‌خونه",
    L"مادرجنده",
    L"ننهجنده",
    L"ننهقحبه",
    L"قحبه",
    L"لاشی",
    L"دیوس",
    L"دیوث",
    L"بیناموس",
    L"بیشرف",
    L"بیغیرت",
    L"خایمال",
    L"خایه‌مال",
    L"خایه",
    L"پدرسگ",
    L"تخمسگ",
    L"سگپدر",
    L"پدرسوخته",
    L"حرومزاده",
    L"حرامزاده",
    L"اوبی",
    L"چسخور",
    L"عنتر",
    L"میگام",
    L"گایید",
    L"گاییده",
    L"بگام",
    L"بگایی",
    L"گاییدی",
    L"شاشیدم",

    // --- English ---
    L"fuck",
    L"fucking",
    L"fucker",
    L"fucked",
    L"fucks",
    L"motherfuck",
    L"shit",
    L"bullshit",
    L"shitty",
    L"bitch",
    L"bitches",
    L"bitchy",
    L"asshole",
    L"dumbass",
    L"jackass",
    L"bastard",
    L"dick",
    L"dickhead",
    L"pussy",
    L"cunt",
    L"cock",
    L"cocksucker",
    L"whore",
    L"slut",
    L"faggot",
    L"fag",
    L"nigger",
    L"nigga",

    // --- Pinglish (پینگلیسی) ---
    L"kos",
    L"koss",
    L"kose",
    L"koskesh",
    L"kosskesh",
    L"koskhol",
    L"koslis",
    L"kharkos",
    L"kharkose",
    L"kharkosseh",
    L"kosmoghz",
    L"kir",
    L"kiri",
    L"kiram",
    L"kiresh",
    L"kiret",
    L"koon",
    L"kooni",
    L"koonde",
    L"koonkesh",
    L"koonpar",
    L"koonpare",
    L"jende",
    L"jendeh",
    L"jendebaz",
    L"madarjende",
    L"nanejende",
    L"dayos",
    L"dayoos",
    L"dayus",
    L"dios",
    L"dyos",
    L"dayyos",
    L"binamoos",
    L"binamus",
    L"bisharaf",
    L"bighirat",
    L"bigheyrat",
    L"khaye",
    L"khayemal",
    L"tokhmi",
    L"tokhmesag",
    L"pedarsag",
    L"sagpedar",
    L"haroomzadeh",
    L"haromzadeh",
    L"haramzadeh",
    L"lashi",
    L"lashee",
    L"obi",
    L"obee",
    L"gayidam",
    L"gaidam",
    L"gayedam",
    L"migam",
    L"begam",
    L"begayi",
    L"gayide",
    L"gayideh",
    L"ghahbeh",
    L"ghahbe"
};

// Strict short words that must be matched as whole words to avoid false positives:
// e.g. "کس", "کص", "کون", "ass"
const wchar_t* kStrictExactWords[] = {
    L"کس",
    L"کص",
    L"کون",
    L"ass",
    L"goh",
    L"chos"
};

// Whitelist / False-positive exceptions
const wchar_t* kWhitelistWords[] = {
    L"کلاس",   // class
    L"کتاب",   // book
    L"کوسه",   // shark
    L"کاسه",   // bowl
    L"کاکتوس", // cactus
    L"عکاس",   // photographer
    L"دستگیر", // arrest
    L"گیر",    // caught/stuck
    L"سیر",    // garlic
    L"مسیر",   // path
    L"اکسیر",  // elixir
    L"pass",
    L"glass",
    L"class",
    L"grass",
    L"assign",
    L"assume",
    L"classic"
};

bool IsWhitelisted(const std::wstring& word)
{
    for (const auto* w : kWhitelistWords)
    {
        if (word == w)
            return true;
    }
    return false;
}

bool CheckTokenIsProfane(const std::wstring& rawToken)
{
    if (rawToken.empty())
        return false;

    std::wstring cleaned = CleanString(rawToken);
    std::wstring collapsed = CollapseRepeats(cleaned);

    if (IsWhitelisted(cleaned) || IsWhitelisted(collapsed))
        return false;

    // Check exact short words
    for (const auto* exact : kStrictExactWords)
    {
        if (cleaned == exact || collapsed == exact)
            return true;
    }

    // Check forbidden stems
    for (const auto* stem : kForbiddenStems)
    {
        if (cleaned.find(stem) != std::wstring::npos || collapsed.find(stem) != std::wstring::npos)
        {
            return true;
        }
    }

    return false;
}

// Tokenizes input into words while preserving delimiters
struct TextPart
{
    std::wstring text;
    bool isWord;
};

std::vector<TextPart> Tokenize(const std::wstring& str)
{
    std::vector<TextPart> parts;
    size_t i = 0;
    while (i < str.size())
    {
        wchar_t c = str[i];
        if (iswspace(c) || iswpunct(c))
        {
            std::wstring delim;
            while (i < str.size() && (iswspace(str[i]) || iswpunct(str[i])))
            {
                delim.push_back(str[i]);
                i++;
            }
            parts.push_back({ delim, false });
        }
        else
        {
            std::wstring word;
            while (i < str.size() && !iswspace(str[i]) && !iswpunct(str[i]))
            {
                word.push_back(str[i]);
                i++;
            }
            parts.push_back({ word, true });
        }
    }
    return parts;
}

} // namespace

bool ContainsProfanity(const std::wstring& textWide)
{
    if (textWide.empty())
        return false;

    // 1. Check individual tokens
    auto parts = Tokenize(textWide);
    for (const auto& p : parts)
    {
        if (p.isWord && CheckTokenIsProfane(p.text))
        {
            return true;
        }
    }

    // 2. Check full collapsed string without spaces to catch spaced-out evasion: e.g. "k o s", "f u c k", "ک ی ر"
    std::wstring squashed = CleanString(textWide);
    std::wstring noSpaces;
    noSpaces.reserve(squashed.size());
    for (wchar_t c : squashed)
    {
        if (!iswspace(c))
            noSpaces.push_back(c);
    }
    std::wstring noSpacesCollapsed = CollapseRepeats(noSpaces);

    for (const auto* stem : kForbiddenStems)
    {
        size_t stemLen = wcslen(stem);
        if (stemLen >= 3) // Only match stems with length >= 3 in squashed stream to avoid overmatching
        {
            if (noSpaces.find(stem) != std::wstring::npos || noSpacesCollapsed.find(stem) != std::wstring::npos)
            {
                return true;
            }
        }
    }

    return false;
}

bool ContainsProfanity(const std::string& textUtf8)
{
    if (textUtf8.empty())
        return false;
    return ContainsProfanity(Persian::Utf8ToWide(textUtf8));
}

std::wstring CensorProfanity(const std::wstring& textWide)
{
    if (textWide.empty())
        return textWide;

    auto parts = Tokenize(textWide);
    std::wstring result;
    for (const auto& p : parts)
    {
        if (p.isWord && CheckTokenIsProfane(p.text))
        {
            result += L"***";
        }
        else
        {
            result += p.text;
        }
    }
    return result;
}

std::string CensorProfanity(const std::string& textUtf8)
{
    if (textUtf8.empty())
        return textUtf8;
    std::wstring wide = Persian::Utf8ToWide(textUtf8);
    std::wstring censored = CensorProfanity(wide);
    return Persian::WideToUtf8(censored);
}

} // namespace ProfanityFilter

#pragma once

#include <string>

namespace ProfanityFilter
{
    // Checks if the given UTF-8 text contains vulgar / profane language (Persian, English, or Pinglish)
    bool ContainsProfanity(const std::string& textUtf8);

    // Checks if the given wide text contains vulgar / profane language
    bool ContainsProfanity(const std::wstring& textWide);

    // Replaces vulgar words in the given UTF-8 text with asterisks (e.g. "***")
    std::string CensorProfanity(const std::string& textUtf8);

    // Replaces vulgar words in the given wide text with asterisks
    std::wstring CensorProfanity(const std::wstring& textWide);
}

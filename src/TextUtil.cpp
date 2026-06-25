#define WIN32_LEAN_AND_MEAN

#include "TextUtil.h"

#include <windows.h>

namespace ojfix {

std::string ToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return std::string();
    }

    const int requiredSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (requiredSize <= 0) {
        return std::string();
    }

    std::string utf8Text(static_cast<size_t>(requiredSize), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        &utf8Text[0],
        requiredSize,
        nullptr,
        nullptr);

    return utf8Text;
}

std::wstring FromUtf8(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }

    const int requiredSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0);

    if (requiredSize <= 0) {
        return std::wstring();
    }

    std::wstring wideText(static_cast<size_t>(requiredSize), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        &wideText[0],
        requiredSize);

    return wideText;
}

} // namespace ojfix

#pragma once

#include <string>

namespace ojfix {

std::string JsonEscapeUtf8(const std::string& text);
std::string JsonEscapeWide(const std::wstring& text);
bool TryExtractJsonStringFieldAfter(
    const std::string& json,
    size_t startIndex,
    const char* fieldName,
    std::string& value,
    std::string& error);

} // namespace ojfix

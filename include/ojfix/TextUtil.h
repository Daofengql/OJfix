#pragma once

#include <string>

namespace ojfix {

std::string ToUtf8(const std::wstring& text);
std::wstring FromUtf8(const std::string& text);

} // namespace ojfix

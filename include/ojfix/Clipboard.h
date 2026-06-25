#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>

namespace ojfix {

bool ReadClipboardPlainText(HWND owner, std::wstring& text);
bool SetClipboardUnicodeText(HWND owner, const std::wstring& text);

} // namespace ojfix

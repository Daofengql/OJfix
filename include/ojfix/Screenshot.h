#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>

namespace ojfix {

bool CaptureVirtualScreenPngBase64(std::string& pngBase64, std::string& error);

} // namespace ojfix

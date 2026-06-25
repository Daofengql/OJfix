#define WIN32_LEAN_AND_MEAN

#include "Logger.h"

#include <windows.h>

#include <cstdio>
#include <iostream>
#include <string>

namespace {

std::wstring LogFilePath()
{
    wchar_t localAppData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData));
    if (length == 0 || length >= ARRAYSIZE(localAppData)) {
        return L"OJfix.log";
    }

    std::wstring directory = std::wstring(localAppData) + L"\\OJfix";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\OJfix.log";
}

void AppendLogLine(const std::string& level, const std::string& message)
{
    const std::wstring path = LogFilePath();
    HANDLE file = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    std::string line = "[" + ojfix::CurrentTimeText() + "] [" + level + "] " + message + "\r\n";
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
}

} // namespace

namespace ojfix {

void WriteInfo(const std::string& message)
{
    AppendLogLine("INFO", message);
#ifdef _CONSOLE
    std::cout << message << std::endl;
#else
    (void)message;
#endif
}

void WriteError(const std::string& message)
{
    AppendLogLine("ERROR", message);
#ifdef _CONSOLE
    std::cerr << message << std::endl;
#else
    (void)message;
#endif
}

std::string CurrentTimeText()
{
    SYSTEMTIME time = {};
    GetLocalTime(&time);

    char buffer[32] = {};
    sprintf_s(
        buffer,
        "%04u-%02u-%02u %02u:%02u:%02u",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond);

    return buffer;
}

} // namespace ojfix

#define WIN32_LEAN_AND_MEAN

#include "AppSettings.h"

#include "AppConfig.h"
#include "TextUtil.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ojfix {
namespace {

std::wstring Trim(const std::wstring& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) {
        return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
    });

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) {
        return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
    }).base();

    if (first >= last) {
        return std::wstring();
    }

    return std::wstring(first, last);
}

std::wstring ModuleDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::wstring();
        }

        if (length < buffer.size() - 1) {
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    std::wstring path(buffer.data(), length);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return std::wstring();
    }

    return path.substr(0, slash);
}

std::wstring ConfigPath()
{
    const std::wstring directory = ModuleDirectory();
    if (directory.empty()) {
        return L"ojfix.ini";
    }

    return directory + L"\\ojfix.ini";
}

std::wstring ReadEnvironmentString(const wchar_t* name)
{
    DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return std::wstring();
    }

    std::wstring value(required, L'\0');
    DWORD written = GetEnvironmentVariableW(name, &value[0], required);
    if (written == 0 || written >= required) {
        return std::wstring();
    }

    value.resize(written);
    return Trim(value);
}

std::wstring ReadIniString(
    const std::wstring& configPath,
    const wchar_t* section,
    const wchar_t* key)
{
    std::vector<wchar_t> buffer(4096, L'\0');
    DWORD written = GetPrivateProfileStringW(
        section,
        key,
        L"",
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        configPath.c_str());

    if (written == 0) {
        return std::wstring();
    }

    if (written >= buffer.size() - 2) {
        buffer.resize(buffer.size() * 2, L'\0');
        written = GetPrivateProfileStringW(
            section,
            key,
            L"",
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            configPath.c_str());
    }

    return Trim(std::wstring(buffer.data(), written));
}

std::wstring ReadSettingWide(
    const std::wstring& configPath,
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* environmentName,
    const wchar_t* defaultValue)
{
    std::wstring value = ReadEnvironmentString(environmentName);
    if (!value.empty()) {
        return value;
    }

    value = ReadIniString(configPath, section, key);
    if (!value.empty()) {
        return value;
    }

    return defaultValue == nullptr ? std::wstring() : std::wstring(defaultValue);
}

ApiSettings LoadApiSettings()
{
    const std::wstring configPath = ConfigPath();

    ApiSettings settings;
    settings.primaryApiBaseUrl = ReadSettingWide(
        configPath,
        L"api",
        L"primary_base_url",
        L"OJFIX_PRIMARY_API_BASE_URL",
        kDefaultPrimaryApiBaseUrl);
    settings.fallbackApiBaseUrl = ReadSettingWide(
        configPath,
        L"api",
        L"fallback_base_url",
        L"OJFIX_FALLBACK_API_BASE_URL",
        kDefaultFallbackApiBaseUrl);
    settings.apiKey = ToUtf8(ReadSettingWide(
        configPath,
        L"api",
        L"key",
        L"OJFIX_API_KEY",
        L""));
    settings.model = ToUtf8(ReadSettingWide(
        configPath,
        L"api",
        L"model",
        L"OJFIX_API_MODEL",
        FromUtf8(kDefaultApiModel).c_str()));

    return settings;
}

} // namespace

const ApiSettings& GetApiSettings()
{
    static const ApiSettings settings = LoadApiSettings();
    return settings;
}

} // namespace ojfix

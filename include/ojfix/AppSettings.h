#pragma once

#include <string>

namespace ojfix {

struct ApiSettings {
    std::wstring primaryApiBaseUrl;
    std::wstring fallbackApiBaseUrl;
    std::string apiKey;
    std::string model;
};

const ApiSettings& GetApiSettings();

} // namespace ojfix

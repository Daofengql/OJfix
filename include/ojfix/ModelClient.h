#pragma once

#include <string>

namespace ojfix {

bool RequestModelAnswer(
    const std::wstring& clipboardText,
    const std::wstring& systemPrompt,
    std::wstring& answer,
    std::string& error);

bool RequestModelAnswerFromImage(
    const std::string& pngBase64,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    std::wstring& answer,
    std::string& error);

} // namespace ojfix

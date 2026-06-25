#define WIN32_LEAN_AND_MEAN

#include "ModelClient.h"

#include "AppConfig.h"
#include "AppSettings.h"
#include "JsonUtil.h"
#include "Logger.h"
#include "TextUtil.h"

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace ojfix {
namespace {

constexpr int kPrimaryEndpointIndex = 0;
constexpr int kFallbackEndpointIndex = 1;

std::atomic<int> g_activeApiEndpointIndex = kPrimaryEndpointIndex;
std::atomic<ULONGLONG> g_fallbackLockedUntilMs = 0;

struct WinHttpHandle {
    HINTERNET value = nullptr;

    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : value(handle) {}
    ~WinHttpHandle()
    {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    operator HINTERNET() const
    {
        return value;
    }
};

const std::wstring& ApiBaseUrl(const ApiSettings& settings, int endpointIndex)
{
    return endpointIndex == kFallbackEndpointIndex
        ? settings.fallbackApiBaseUrl
        : settings.primaryApiBaseUrl;
}

const char* ApiEndpointName(int endpointIndex)
{
    return endpointIndex == kFallbackEndpointIndex ? "fallback" : "primary";
}

std::string FormatWinHttpError(const std::string& operation, DWORD errorCode)
{
    return operation + " failed. Error: " + std::to_string(errorCode);
}

bool IsConnectionFailureError(DWORD errorCode)
{
    switch (errorCode) {
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_SECURE_FAILURE:
        return true;
    default:
        return false;
    }
}

void SetWinHttpError(
    std::string& error,
    bool& connectionFailure,
    const std::string& operation,
    DWORD errorCode)
{
    error = FormatWinHttpError(operation, errorCode);
    if (IsConnectionFailureError(errorCode)) {
        connectionFailure = true;
    }
}

bool IsRetryableServerStatus(DWORD statusCode)
{
    return statusCode == 408 ||
        statusCode == 429 ||
        statusCode == 502 ||
        statusCode == 503 ||
        statusCode == 504;
}

int TimeoutForEndpoint(int endpointIndex, int primaryTimeout, int fallbackTimeout)
{
    return endpointIndex == kFallbackEndpointIndex ? fallbackTimeout : primaryTimeout;
}

void LockFallbackForCooldown()
{
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG newLockedUntil = now + kFallbackApiCooldownMs;

    ULONGLONG currentLockedUntil = g_fallbackLockedUntilMs.load();
    while (currentLockedUntil < newLockedUntil &&
           !g_fallbackLockedUntilMs.compare_exchange_weak(currentLockedUntil, newLockedUntil)) {
    }

    g_activeApiEndpointIndex = kFallbackEndpointIndex;
}

bool HasFallbackEndpoint(const ApiSettings& settings)
{
    return !settings.fallbackApiBaseUrl.empty();
}

int SelectActiveEndpoint(const ApiSettings& settings)
{
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG lockedUntil = g_fallbackLockedUntilMs.load();
    const int endpointIndex = (HasFallbackEndpoint(settings) && lockedUntil != 0 && now < lockedUntil)
        ? kFallbackEndpointIndex
        : kPrimaryEndpointIndex;

    g_activeApiEndpointIndex = endpointIndex;
    return endpointIndex;
}

bool ExtractFirstMessageContent(const std::string& responseBody, std::wstring& content, std::string& error)
{
    const size_t choicesIndex = responseBody.find("\"choices\"");
    if (choicesIndex == std::string::npos) {
        error = "Response JSON does not contain choices.";
        return false;
    }

    const size_t messageIndex = responseBody.find("\"message\"", choicesIndex);
    if (messageIndex == std::string::npos) {
        error = "Response JSON does not contain choices[0].message.";
        return false;
    }

    const size_t contentIndex = responseBody.find("\"content\"", messageIndex);
    if (contentIndex == std::string::npos) {
        error = "Response JSON does not contain choices[0].message.content.";
        return false;
    }

    std::string utf8Content;
    if (!TryExtractJsonStringFieldAfter(responseBody, contentIndex, "content", utf8Content, error)) {
        return false;
    }

    content = FromUtf8(utf8Content);
    return true;
}

bool ExtractStreamContentPiece(const std::string& eventJson, std::string& utf8Piece, std::string& error)
{
    utf8Piece.clear();

    const size_t choicesIndex = eventJson.find("\"choices\"");
    const size_t searchStart = choicesIndex == std::string::npos ? 0 : choicesIndex;
    const size_t deltaIndex = eventJson.find("\"delta\"", searchStart);
    if (deltaIndex != std::string::npos) {
        std::string candidate;
        std::string localError;
        if (TryExtractJsonStringFieldAfter(eventJson, deltaIndex, "content", candidate, localError)) {
            utf8Piece = candidate;
            return true;
        }

        if (!localError.empty() && localError.find("not a string") == std::string::npos) {
            error = localError;
            return false;
        }

        return true;
    }

    const size_t messageIndex = eventJson.find("\"message\"", searchStart);
    if (messageIndex != std::string::npos) {
        std::string candidate;
        std::string localError;
        if (TryExtractJsonStringFieldAfter(eventJson, messageIndex, "content", candidate, localError)) {
            utf8Piece = candidate;
            return true;
        }

        if (!localError.empty() && localError.find("not a string") == std::string::npos) {
            error = localError;
            return false;
        }

        return true;
    }

    const size_t errorIndex = eventJson.find("\"error\"");
    if (errorIndex != std::string::npos) {
        std::string errorMessage;
        if (TryExtractJsonStringFieldAfter(eventJson, errorIndex, "message", errorMessage, error)) {
            error = "API error: " + errorMessage;
        } else if (error.empty()) {
            error = "API returned an error object.";
        }
        return false;
    }

    return true;
}

bool ProcessSseEvent(const std::string& eventText, std::string& answerUtf8, bool& sawDone, std::string& error)
{
    size_t lineStart = 0;
    while (lineStart <= eventText.size()) {
        size_t lineEnd = eventText.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = eventText.size();
        }

        std::string line = eventText.substr(lineStart, lineEnd - lineStart);
        if (line.rfind("data:", 0) == 0) {
            std::string payload = line.substr(5);
            if (!payload.empty() && payload.front() == ' ') {
                payload.erase(payload.begin());
            }

            if (payload == "[DONE]") {
                sawDone = true;
                return true;
            }

            if (!payload.empty()) {
                std::string piece;
                if (!ExtractStreamContentPiece(payload, piece, error)) {
                    return false;
                }

                answerUtf8 += piece;
            }
        }

        if (lineEnd == eventText.size()) {
            break;
        }

        lineStart = lineEnd + 1;
    }

    return true;
}

bool ProcessSseBytes(
    const char* data,
    DWORD size,
    std::string& pendingLine,
    std::string& pendingEvent,
    std::string& answerUtf8,
    bool& sawDone,
    std::string& error)
{
    for (DWORD index = 0; index < size; ++index) {
        const char ch = data[index];
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            if (pendingLine.empty()) {
                if (!pendingEvent.empty()) {
                    if (!ProcessSseEvent(pendingEvent, answerUtf8, sawDone, error)) {
                        return false;
                    }

                    pendingEvent.clear();
                }

                if (sawDone) {
                    return true;
                }
            } else {
                pendingEvent += pendingLine;
                pendingEvent.push_back('\n');
                pendingLine.clear();
            }

            continue;
        }

        pendingLine.push_back(ch);
    }

    return true;
}

bool FinishSseStream(
    std::string& pendingLine,
    std::string& pendingEvent,
    std::string& answerUtf8,
    bool& sawDone,
    std::string& error)
{
    if (!pendingLine.empty()) {
        pendingEvent += pendingLine;
        pendingEvent.push_back('\n');
        pendingLine.clear();
    }

    if (!pendingEvent.empty()) {
        if (!ProcessSseEvent(pendingEvent, answerUtf8, sawDone, error)) {
            return false;
        }

        pendingEvent.clear();
    }

    return true;
}

std::string BuildChatCompletionsRequestBody(
    const std::wstring& clipboardText,
    const std::wstring& systemPrompt,
    const std::string& model)
{
    std::string body;
    body.reserve(ToUtf8(clipboardText).size() + 512);
    body += "{";
    body += "\"model\":\"";
    body += JsonEscapeUtf8(model);
    body += "\",";
    body += "\"messages\":[";
    body += "{\"role\":\"system\",\"content\":\"";
    body += JsonEscapeWide(systemPrompt);
    body += "\"},";
    body += "{\"role\":\"user\",\"content\":\"";
    body += JsonEscapeWide(clipboardText);
    body += "\"}";
    body += "],";
    body += "\"temperature\":0.2,";
    body += "\"stream\":";
    body += kUseStreamingResponse ? "true" : "false";
    body += "}";

    return body;
}

std::string BuildImageChatCompletionsRequestBody(
    const std::string& pngBase64,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    const std::string& model)
{
    std::string body;
    body.reserve(pngBase64.size() + ToUtf8(systemPrompt).size() + ToUtf8(userPrompt).size() + 1024);
    body += "{";
    body += "\"model\":\"";
    body += JsonEscapeUtf8(model);
    body += "\",";
    body += "\"messages\":[";
    body += "{\"role\":\"system\",\"content\":\"";
    body += JsonEscapeWide(systemPrompt);
    body += "\"},";
    body += "{\"role\":\"user\",\"content\":[";
    body += "{\"type\":\"text\",\"text\":\"";
    body += JsonEscapeWide(userPrompt);
    body += "\"},";
    body += "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,";
    body += pngBase64;
    body += "\"}}";
    body += "]}";
    body += "],";
    body += "\"temperature\":0.2,";
    body += "\"stream\":";
    body += kUseStreamingResponse ? "true" : "false";
    body += "}";

    return body;
}

std::wstring BuildChatCompletionsPath(const URL_COMPONENTSW& components)
{
    std::wstring path;
    if (components.dwUrlPathLength > 0) {
        path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    }

    while (!path.empty() && path.back() == L'/') {
        path.pop_back();
    }

    path += L"/chat/completions";

    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    return path;
}

bool RequestModelAnswerFromEndpoint(
    const ApiSettings& settings,
    int endpointIndex,
    const std::string& requestBody,
    std::wstring& answer,
    std::string& error,
    bool& connectionFailure)
{
    connectionFailure = false;

    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    wchar_t extraInfo[1024] = {};

    URL_COMPONENTSW components = {};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostName;
    components.dwHostNameLength = ARRAYSIZE(hostName);
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = ARRAYSIZE(urlPath);
    components.lpszExtraInfo = extraInfo;
    components.dwExtraInfoLength = ARRAYSIZE(extraInfo);

    const std::wstring& apiBaseUrl = ApiBaseUrl(settings, endpointIndex);
    if (apiBaseUrl.empty()) {
        error = std::string(ApiEndpointName(endpointIndex)) + " API base URL is not configured.";
        return false;
    }

    if (!WinHttpCrackUrl(apiBaseUrl.c_str(), 0, 0, &components)) {
        error = "Invalid API base URL. WinHTTP error: " + std::to_string(GetLastError());
        return false;
    }

    const bool useHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
    const std::wstring requestPath = BuildChatCompletionsPath(components);

    WinHttpHandle session(WinHttpOpen(
        L"OJfix/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session) {
        error = "WinHttpOpen failed. Error: " + std::to_string(GetLastError());
        return false;
    }

    WinHttpHandle connection(WinHttpConnect(session, hostName, components.nPort, 0));
    if (!connection) {
        SetWinHttpError(error, connectionFailure, "WinHttpConnect", GetLastError());
        return false;
    }

    WinHttpHandle request(WinHttpOpenRequest(
        connection,
        L"POST",
        requestPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        useHttps ? WINHTTP_FLAG_SECURE : 0));
    if (!request) {
        error = "WinHttpOpenRequest failed. Error: " + std::to_string(GetLastError());
        return false;
    }

    if (!WinHttpSetTimeouts(
        request,
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpResolveTimeoutMs, kFallbackHttpResolveTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpConnectTimeoutMs, kFallbackHttpConnectTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpSendTimeoutMs, kFallbackHttpSendTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpResponseTimeoutMs, kFallbackHttpResponseTimeoutMs))) {
        SetWinHttpError(error, connectionFailure, "WinHttpSetTimeouts", GetLastError());
        return false;
    }

    const std::wstring headers =
        L"Content-Type: application/json; charset=utf-8\r\n"
        L"Accept: text/event-stream\r\n"
        L"Authorization: Bearer " + FromUtf8(settings.apiKey) + L"\r\n";

    const DWORD bodySize = static_cast<DWORD>(requestBody.size());
    if (!WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        bodySize == 0 ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(requestBody.data()),
        bodySize,
        bodySize,
        0)) {
        SetWinHttpError(error, connectionFailure, "WinHttpSendRequest", GetLastError());
        return false;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        SetWinHttpError(error, connectionFailure, "WinHttpReceiveResponse", GetLastError());
        return false;
    }

    if (!WinHttpSetTimeouts(
        request,
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpResolveTimeoutMs, kFallbackHttpResolveTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpConnectTimeoutMs, kFallbackHttpConnectTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpSendTimeoutMs, kFallbackHttpSendTimeoutMs),
        TimeoutForEndpoint(endpointIndex, kPrimaryHttpStreamIdleTimeoutMs, kFallbackHttpStreamIdleTimeoutMs))) {
        SetWinHttpError(error, connectionFailure, "WinHttpSetTimeouts(stream)", GetLastError());
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX)) {
        SetWinHttpError(error, connectionFailure, "WinHttpQueryHeaders", GetLastError());
        return false;
    }

    const bool shouldParseStream = kUseStreamingResponse && statusCode >= 200 && statusCode < 300;
    std::string responseBody;
    std::string answerUtf8;
    std::string pendingLine;
    std::string pendingEvent;
    bool sawDone = false;

    while (true) {
        DWORD availableSize = 0;
        if (!WinHttpQueryDataAvailable(request, &availableSize)) {
            SetWinHttpError(error, connectionFailure, "WinHttpQueryDataAvailable", GetLastError());
            return false;
        }

        if (availableSize == 0) {
            break;
        }

        std::vector<char> buffer(static_cast<size_t>(availableSize));
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request, buffer.data(), availableSize, &bytesRead)) {
            SetWinHttpError(error, connectionFailure, "WinHttpReadData", GetLastError());
            return false;
        }

        responseBody.append(buffer.data(), bytesRead);

        if (shouldParseStream &&
            !ProcessSseBytes(
                buffer.data(),
                bytesRead,
                pendingLine,
                pendingEvent,
                answerUtf8,
                sawDone,
                error)) {
            return false;
        }

        if (sawDone) {
            break;
        }
    }

    if (statusCode < 200 || statusCode >= 300) {
        std::string responsePreview = responseBody.substr(0, 500);
        if (endpointIndex == kPrimaryEndpointIndex && IsRetryableServerStatus(statusCode)) {
            connectionFailure = true;
        }
        error = "HTTP " + std::to_string(statusCode) + ": " + responsePreview;
        return false;
    }

    if (shouldParseStream) {
        if (!FinishSseStream(pendingLine, pendingEvent, answerUtf8, sawDone, error)) {
            return false;
        }

        if (!answerUtf8.empty()) {
            answer = FromUtf8(answerUtf8);
            return true;
        }
    }

    return ExtractFirstMessageContent(responseBody, answer, error);
}

} // namespace

bool RequestModelAnswerWithBody(
    const ApiSettings& settings,
    const std::string& requestBody,
    std::wstring& answer,
    std::string& error)
{
    if (settings.apiKey.empty()) {
        error = "API key is not configured. Set OJFIX_API_KEY or create ojfix.ini next to OJfix.exe.";
        return false;
    }

    if (settings.primaryApiBaseUrl.empty()) {
        error = "Primary API base URL is not configured.";
        return false;
    }

    const int endpointIndex = SelectActiveEndpoint(settings);

    bool connectionFailure = false;
    std::string attemptError;
    if (RequestModelAnswerFromEndpoint(
        settings,
        endpointIndex,
        requestBody,
        answer,
        attemptError,
        connectionFailure)) {
        return true;
    }

    if (endpointIndex != kPrimaryEndpointIndex) {
        error = std::string("Fallback API request failed: ") + attemptError;
        return false;
    }

    if (!connectionFailure) {
        error = std::string("Primary API request failed: ") + attemptError;
        return false;
    }

    WriteError("Primary API connection failure: " + attemptError);

    if (!HasFallbackEndpoint(settings)) {
        error = "Primary API connection failed and fallback API base URL is not configured: " + attemptError;
        return false;
    }

    LockFallbackForCooldown();
    WriteInfo("Primary API temporarily locked out; retrying with fallback API.");

    bool fallbackConnectionFailure = false;
    std::string fallbackError;
    if (RequestModelAnswerFromEndpoint(
        settings,
        kFallbackEndpointIndex,
        requestBody,
        answer,
        fallbackError,
        fallbackConnectionFailure)) {
        return true;
    }

    if (fallbackConnectionFailure) {
        LockFallbackForCooldown();
    }

    error =
        "Primary API connection failed and fallback API request failed: " +
        fallbackError;
    return false;
}

bool RequestModelAnswer(
    const std::wstring& clipboardText,
    const std::wstring& systemPrompt,
    std::wstring& answer,
    std::string& error)
{
    const ApiSettings& settings = GetApiSettings();
    return RequestModelAnswerWithBody(
        settings,
        BuildChatCompletionsRequestBody(clipboardText, systemPrompt, settings.model),
        answer,
        error);
}

bool RequestModelAnswerFromImage(
    const std::string& pngBase64,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    std::wstring& answer,
    std::string& error)
{
    const ApiSettings& settings = GetApiSettings();
    return RequestModelAnswerWithBody(
        settings,
        BuildImageChatCompletionsRequestBody(pngBase64, systemPrompt, userPrompt, settings.model),
        answer,
        error);
}

} // namespace ojfix

#define WIN32_LEAN_AND_MEAN

#include "ClipboardMonitorApp.h"

#include "AppConfig.h"
#include "Clipboard.h"
#include "Logger.h"
#include "ModelClient.h"
#include "Notifications.h"
#include "resource.h"
#include "Screenshot.h"
#include "TextUtil.h"

#include <windows.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#pragma comment(lib, "user32.lib")

namespace ojfix {
namespace {

std::wstring g_lastText;
bool g_hasLastText = false;
std::mutex g_clipboardTextMutex;
std::atomic_bool g_requestRunning = false;
std::atomic_size_t g_currentSystemPromptIndex = kDefaultSystemPromptIndex;
std::atomic_bool g_notificationsEnabled = false;
std::atomic_bool g_clipboardListenerRegistered = false;
std::atomic_bool g_submitHotkeyRegistered = false;
std::atomic_bool g_screenshotHotkeyRegistered = false;
std::atomic_bool g_promptSwitchHotkeyRegistered = false;
std::atomic_bool g_notificationToggleHotkeyRegistered = false;
std::atomic<DWORD> g_exitHotkeyThreadId = 0;
std::atomic_bool g_exitHotkeyThreadFinished = false;

size_t NormalizePromptIndex(size_t index)
{
    if (index >= kSystemPrompts.size()) {
        return kDefaultSystemPromptIndex;
    }

    return index;
}

std::string CurrentPromptName()
{
    const size_t index = NormalizePromptIndex(g_currentSystemPromptIndex.load());
    return kSystemPromptNames[index];
}

std::wstring CurrentSystemPrompt()
{
    const size_t index = NormalizePromptIndex(g_currentSystemPromptIndex.load());
    return kSystemPrompts[index];
}

void PrintClipboardChange(const std::wstring& text)
{
#ifdef _CONSOLE
    std::cout << "\n[" << CurrentTimeText() << "] Clipboard text changed";
    std::cout << " (" << text.size() << " wchar_t)\n";
    std::cout << ToUtf8(text) << "\n";
    std::cout << "----" << std::endl;
#else
    (void)text;
#endif
}

void CaptureInitialClipboardText(HWND owner)
{
    std::wstring text;
    if (ReadClipboardPlainText(owner, text)) {
        std::lock_guard<std::mutex> lock(g_clipboardTextMutex);
        g_lastText = text;
        g_hasLastText = true;
        WriteInfo("Initial clipboard text captured. Waiting for changes...");
        return;
    }

    WriteInfo("No initial plain text found. Waiting for changes...");
}

void HandleClipboardUpdate(HWND owner)
{
    std::wstring text;
    if (!ReadClipboardPlainText(owner, text)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_clipboardTextMutex);
        if (g_hasLastText && text == g_lastText) {
            return;
        }

        g_lastText = text;
        g_hasLastText = true;
    }

    PrintClipboardChange(text);
}

void HandleSubmitHotkey(HWND owner)
{
    if (g_requestRunning.exchange(true)) {
        WriteInfo("Model request is already running.");
        return;
    }

    std::wstring clipboardText;
    std::wstring currentClipboardText;
    const bool hasCurrentClipboardText =
        ReadClipboardPlainText(owner, currentClipboardText) && !currentClipboardText.empty();

    {
        std::lock_guard<std::mutex> lock(g_clipboardTextMutex);
        if (hasCurrentClipboardText) {
            g_lastText = currentClipboardText;
            g_hasLastText = true;
        }

        if (!g_hasLastText || g_lastText.empty()) {
            WriteInfo("No clipboard text is available to submit.");
            g_requestRunning = false;
            return;
        }

        clipboardText = g_lastText;
    }

    const std::wstring systemPrompt = CurrentSystemPrompt();
    const std::string promptName = CurrentPromptName();

    std::thread([owner, clipboardText, systemPrompt, promptName]() {
        WriteInfo("Sending clipboard text to model with prompt: " + promptName);

        std::wstring answer;
        std::string error;
        if (!RequestModelAnswer(clipboardText, systemPrompt, answer, error)) {
            WriteError("Model request failed: " + error);
            g_requestRunning = false;
            return;
        }

        if (!SetClipboardUnicodeText(owner, answer)) {
            WriteError("Failed to replace clipboard text. Error: " + std::to_string(GetLastError()));
            g_requestRunning = false;
            return;
        }

        {
            std::lock_guard<std::mutex> lock(g_clipboardTextMutex);
            g_lastText = answer;
            g_hasLastText = true;
        }

        WriteInfo("Model response copied to clipboard.");
        g_requestRunning = false;
    }).detach();
}

void HandleScreenshotHotkey(HWND owner)
{
    if (g_requestRunning.exchange(true)) {
        WriteInfo("Model request is already running.");
        return;
    }

    const std::wstring systemPrompt = CurrentSystemPrompt();
    const std::string promptName = CurrentPromptName();

    std::thread([owner, systemPrompt, promptName]() {
        WriteInfo("Capturing screen with GDI...");

        std::string pngBase64;
        std::string error;
        if (!CaptureVirtualScreenPngBase64(pngBase64, error)) {
            WriteError("Screenshot capture failed: " + error);
            g_requestRunning = false;
            return;
        }

        WriteInfo("Sending screenshot to model with prompt: " + promptName);

        std::wstring answer;
        if (!RequestModelAnswerFromImage(pngBase64, systemPrompt, kScreenshotUserPrompt, answer, error)) {
            WriteError("Screenshot model request failed: " + error);
            g_requestRunning = false;
            return;
        }

        if (!SetClipboardUnicodeText(owner, answer)) {
            WriteError("Failed to replace clipboard text. Error: " + std::to_string(GetLastError()));
            g_requestRunning = false;
            return;
        }

        {
            std::lock_guard<std::mutex> lock(g_clipboardTextMutex);
            g_lastText = answer;
            g_hasLastText = true;
        }

        WriteInfo("Screenshot model response copied to clipboard.");
        g_requestRunning = false;
    }).detach();
}

void HandlePromptSwitchHotkey(HWND owner)
{
    const size_t currentIndex = NormalizePromptIndex(g_currentSystemPromptIndex.load());
    const size_t nextIndex = (currentIndex + 1) % kSystemPrompts.size();
    g_currentSystemPromptIndex = nextIndex;
    WriteInfo(std::string("Current prompt switched to: ") + kSystemPromptNames[nextIndex]);
    if (g_notificationsEnabled.load()) {
        ShowNotification(
            owner,
            L"OJfix",
            FromUtf8(std::string("当前 prompt: ") + kSystemPromptNames[nextIndex]),
            NotificationKind::Info);
    }
}

void HandleNotificationToggleHotkey(HWND owner)
{
    if (g_notificationsEnabled.load()) {
        g_notificationsEnabled = false;
        ShutdownNotifications(owner);
        WriteInfo("Prompt switch notifications disabled.");
        return;
    }

    if (!InitializeNotifications(owner)) {
        WriteError("Failed to initialize Windows notifications.");
        return;
    }

    g_notificationsEnabled = true;
    WriteInfo("Prompt switch notifications enabled.");
}

void ExitHotkeyMonitorThread(HWND owner)
{
    g_exitHotkeyThreadId = GetCurrentThreadId();

    if (!RegisterHotKey(nullptr, kExitHotkeyId, kExitHotkeyModifiers, kExitHotkeyVirtualKey)) {
        WriteError(std::string("Failed to register exit hotkey ") + kExitHotkeyText + ". Error: " + std::to_string(GetLastError()));
        g_exitHotkeyThreadId = 0;
        g_exitHotkeyThreadFinished = true;
        return;
    }

    WriteInfo(std::string("Exit hotkey registered: ") + kExitHotkeyText + ".");

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == kExitHotkeyId) {
            WriteInfo("Exit hotkey pressed. Closing OJfix...");
            PostMessageW(owner, WM_CLOSE, 0, 0);
            break;
        }
    }

    UnregisterHotKey(nullptr, kExitHotkeyId);
    g_exitHotkeyThreadId = 0;
    g_exitHotkeyThreadFinished = true;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        if (!AddClipboardFormatListener(window)) {
            WriteError("Failed to register clipboard listener. Error: " + std::to_string(GetLastError()));
        } else {
            g_clipboardListenerRegistered = true;
        }

        if (!RegisterHotKey(window, kSubmitHotkeyId, kSubmitHotkeyModifiers, kSubmitHotkeyVirtualKey)) {
            WriteError(std::string("Failed to register hotkey ") + kSubmitHotkeyText + ". Error: " + std::to_string(GetLastError()));
        } else {
            g_submitHotkeyRegistered = true;
        }

        if (!RegisterHotKey(window, kScreenshotHotkeyId, kScreenshotHotkeyModifiers, kScreenshotHotkeyVirtualKey)) {
            WriteError(std::string("Failed to register hotkey ") + kScreenshotHotkeyText + ". Error: " + std::to_string(GetLastError()));
        } else {
            g_screenshotHotkeyRegistered = true;
        }

        if (!RegisterHotKey(window, kPromptSwitchHotkeyId, kPromptSwitchHotkeyModifiers, kPromptSwitchHotkeyVirtualKey)) {
            WriteError(std::string("Failed to register hotkey ") + kPromptSwitchHotkeyText + ". Error: " + std::to_string(GetLastError()));
        } else {
            g_promptSwitchHotkeyRegistered = true;
        }

        if (!RegisterHotKey(window, kNotificationToggleHotkeyId, kNotificationToggleHotkeyModifiers, kNotificationToggleHotkeyVirtualKey)) {
            WriteError(std::string("Failed to register hotkey ") + kNotificationToggleHotkeyText + ". Error: " + std::to_string(GetLastError()));
        } else {
            g_notificationToggleHotkeyRegistered = true;
        }
        return 0;

    case WM_CLIPBOARDUPDATE:
        HandleClipboardUpdate(window);
        return 0;

    case WM_HOTKEY:
        if (wParam == kSubmitHotkeyId) {
            HandleSubmitHotkey(window);
            return 0;
        }
        if (wParam == kScreenshotHotkeyId) {
            HandleScreenshotHotkey(window);
            return 0;
        }
        if (wParam == kPromptSwitchHotkeyId) {
            HandlePromptSwitchHotkey(window);
            return 0;
        }
        if (wParam == kNotificationToggleHotkeyId) {
            HandleNotificationToggleHotkey(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_DESTROY:
        if (g_notificationToggleHotkeyRegistered.exchange(false)) {
            UnregisterHotKey(window, kNotificationToggleHotkeyId);
        }
        if (g_promptSwitchHotkeyRegistered.exchange(false)) {
            UnregisterHotKey(window, kPromptSwitchHotkeyId);
        }
        if (g_screenshotHotkeyRegistered.exchange(false)) {
            UnregisterHotKey(window, kScreenshotHotkeyId);
        }
        if (g_submitHotkeyRegistered.exchange(false)) {
            UnregisterHotKey(window, kSubmitHotkeyId);
        }
        if (g_clipboardListenerRegistered.exchange(false)) {
            RemoveClipboardFormatListener(window);
        }
        ShutdownNotifications(window);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void StopExitHotkeyThread(std::thread& exitHotkeyThread)
{
    const DWORD exitHotkeyThreadId = g_exitHotkeyThreadId.load();
    if (exitHotkeyThreadId != 0) {
        PostThreadMessageW(exitHotkeyThreadId, WM_QUIT, 0, 0);
    }

    if (exitHotkeyThread.joinable()) {
        exitHotkeyThread.join();
    }
}

} // namespace

int RunClipboardMonitor()
{
#ifdef _CONSOLE
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "OJfix clipboard plain-text monitor" << std::endl;
    std::cout << "Submit hotkey: " << kSubmitHotkeyText << std::endl;
    std::cout << "Screenshot hotkey: " << kScreenshotHotkeyText << std::endl;
    std::cout << "Switch prompt hotkey: " << kPromptSwitchHotkeyText << std::endl;
    std::cout << "Toggle prompt switch notifications hotkey: " << kNotificationToggleHotkeyText << std::endl;
    std::cout << "Exit hotkey: " << kExitHotkeyText << std::endl;
    std::cout << "Default prompt: " << CurrentPromptName() << std::endl;
    std::cout << "Prompt switch notifications: disabled" << std::endl;
    std::cout << "Primary response timeout: " << (kPrimaryHttpResponseTimeoutMs / 1000) << " seconds" << std::endl;
    std::cout << "Primary stream idle timeout: " << (kPrimaryHttpStreamIdleTimeoutMs / 1000) << " seconds" << std::endl;
    std::cout << "Fallback response timeout: " << (kFallbackHttpResponseTimeoutMs / 1000) << " seconds" << std::endl;
    std::cout << "Fallback stream idle timeout: " << (kFallbackHttpStreamIdleTimeoutMs / 1000) << " seconds" << std::endl;
    std::cout << "Fallback cooldown: " << (kFallbackApiCooldownMs / 60000) << " minutes" << std::endl;
    std::cout << "Streaming response: " << (kUseStreamingResponse ? "enabled" : "disabled") << std::endl;
    std::cout << "Press Ctrl+C to stop in Debug." << std::endl;
#endif

    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (singleInstanceMutex == nullptr) {
        WriteError("Failed to create single-instance mutex. Error: " + std::to_string(GetLastError()));
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        WriteInfo("OJfix clipboard monitor is already running.");
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = windowClass.hIcon;

    if (!RegisterClassExW(&windowClass)) {
        WriteError("Failed to register window class. Error: " + std::to_string(GetLastError()));
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        L"OJfix Clipboard Monitor",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr) {
        WriteError("Failed to create message window. Error: " + std::to_string(GetLastError()));
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    CaptureInitialClipboardText(window);

    g_exitHotkeyThreadFinished = false;
    std::thread exitHotkeyThread(ExitHotkeyMonitorThread, window);
    for (int attempt = 0; attempt < 100 &&
         g_exitHotkeyThreadId.load() == 0 &&
         !g_exitHotkeyThreadFinished.load(); ++attempt) {
        Sleep(10);
    }

    MSG message = {};
    while (true) {
        BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }

        if (result == 0) {
            break;
        }

        WriteError("Failed to read window message. Error: " + std::to_string(GetLastError()));
        Sleep(100);
    }

    StopExitHotkeyThread(exitHotkeyThread);
    CloseHandle(singleInstanceMutex);
    return 0;
}

} // namespace ojfix

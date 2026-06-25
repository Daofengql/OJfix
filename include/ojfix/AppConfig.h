#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <array>
#include <cstddef>

namespace ojfix {

constexpr wchar_t kWindowClassName[] = L"OJfixClipboardMonitorWindow";
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\OJfixClipboardMonitor";

constexpr wchar_t kDefaultPrimaryApiBaseUrl[] = L"https://api.openai.com/v1";
constexpr wchar_t kDefaultFallbackApiBaseUrl[] = L"";
constexpr ULONGLONG kFallbackApiCooldownMs = 15ULL * 60ULL * 1000ULL;
constexpr char kDefaultApiModel[] = "gpt-4.1-mini";

constexpr int kPrimaryHttpResolveTimeoutMs = 3000;
constexpr int kPrimaryHttpConnectTimeoutMs = 8000;
constexpr int kPrimaryHttpSendTimeoutMs = 10000;
constexpr int kPrimaryHttpResponseTimeoutMs = 45000;
constexpr int kPrimaryHttpStreamIdleTimeoutMs = 120000;

constexpr int kFallbackHttpResolveTimeoutMs = 15000;
constexpr int kFallbackHttpConnectTimeoutMs = 30000;
constexpr int kFallbackHttpSendTimeoutMs = 30000;
constexpr int kFallbackHttpResponseTimeoutMs = 60000;
constexpr int kFallbackHttpStreamIdleTimeoutMs = 240000;
constexpr bool kUseStreamingResponse = true;

constexpr int kSubmitHotkeyId = 1001;
constexpr UINT kSubmitHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kSubmitHotkeyVirtualKey = '1';
constexpr char kSubmitHotkeyText[] = "Ctrl+Alt+1";

constexpr int kScreenshotHotkeyId = 1005;
constexpr UINT kScreenshotHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kScreenshotHotkeyVirtualKey = '2';
constexpr char kScreenshotHotkeyText[] = "Ctrl+Alt+2";
constexpr wchar_t kScreenshotUserPrompt[] =
    L"请根据截图中的题目内容解答。如果截图中是在线评测题目，请完整识别题意、输入输出和约束后给出最终代码。最终回复只能是纯代码，不要输出 Markdown 代码围栏，不要输出三个反引号，不要输出 ```cpp、```c++、```java、```python 或 ``` 这样的标签。";

constexpr int kPromptSwitchHotkeyId = 1003;
constexpr UINT kPromptSwitchHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kPromptSwitchHotkeyVirtualKey = '3';
constexpr char kPromptSwitchHotkeyText[] = "Ctrl+Alt+3";

constexpr int kNotificationToggleHotkeyId = 1004;
constexpr UINT kNotificationToggleHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kNotificationToggleHotkeyVirtualKey = '4';
constexpr char kNotificationToggleHotkeyText[] = "Ctrl+Alt+4";

constexpr int kExitHotkeyId = 1002;
constexpr UINT kExitHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kExitHotkeyVirtualKey = '5';
constexpr char kExitHotkeyText[] = "Ctrl+Alt+5";

const std::array<const char*, 4> kSystemPromptNames = {
    "Java",
    "MSVC C++",
    "GNU G++ C++17",
    "Python 3"
};

const std::array<const wchar_t*, 4> kSystemPrompts = {
    L"你是在线评测题目解答器。请使用 Java 解答，类名使用 Main。只输出可以直接提交的完整源代码。不要输出解释、思路、标题、Markdown 代码块或任何额外文字。不要输出 Markdown 代码围栏，不要输出三个反引号，不要输出 ```java 或 ``` 这样的标签。代码中不要包含任何注释。",
    L"你是在线评测题目解答器。请使用 MSVC C++ 解答，并注意 MSVC 兼容性。只输出可以直接编译运行的完整源代码。不要输出解释、思路、标题、Markdown 代码块或任何额外文字。不要输出 Markdown 代码围栏，不要输出三个反引号，不要输出 ```cpp、```c++ 或 ``` 这样的标签。代码中不要包含任何注释。",
    L"你是在线评测题目解答器。请使用 GNU G++ C++17 解答。只输出可以直接提交的完整源代码。不要输出解释、思路、标题、Markdown 代码块或任何额外文字。不要输出 Markdown 代码围栏，不要输出三个反引号，不要输出 ```cpp、```c++ 或 ``` 这样的标签。代码中不要包含任何注释。",
    L"你是在线评测题目解答器。请使用 Python 3 解答。只输出可以直接提交的完整源代码。不要输出解释、思路、标题、Markdown 代码块或任何额外文字。不要输出 Markdown 代码围栏，不要输出三个反引号，不要输出 ```python 或 ``` 这样的标签。代码中不要包含任何注释。"
};
constexpr size_t kDefaultSystemPromptIndex = 2;

} // namespace ojfix

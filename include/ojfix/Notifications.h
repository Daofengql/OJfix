#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>

namespace ojfix {

enum class NotificationKind {
    Info,
    Warning,
    Error
};

bool InitializeNotifications(HWND owner);
void ShutdownNotifications(HWND owner);
void ShowNotification(
    HWND owner,
    const std::wstring& title,
    const std::wstring& message,
    NotificationKind kind = NotificationKind::Info);

} // namespace ojfix

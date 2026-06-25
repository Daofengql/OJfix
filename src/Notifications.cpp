#define WIN32_LEAN_AND_MEAN

#include "Notifications.h"

#include "resource.h"

#include <shellapi.h>

#include <cwchar>

#pragma comment(lib, "shell32.lib")

namespace ojfix {
namespace {

constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 1;

void CopyLimited(wchar_t* destination, size_t destinationSize, const std::wstring& source)
{
    if (destinationSize == 0) {
        return;
    }

    const size_t copyLength = source.size() < destinationSize - 1 ? source.size() : destinationSize - 1;
    wmemcpy(destination, source.c_str(), copyLength);
    destination[copyLength] = L'\0';
}

DWORD IconFlag(NotificationKind kind)
{
    switch (kind) {
    case NotificationKind::Warning:
        return NIIF_WARNING;
    case NotificationKind::Error:
        return NIIF_ERROR;
    default:
        return NIIF_INFO;
    }
}

} // namespace

bool InitializeNotifications(HWND owner)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = kTrayIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kTrayCallbackMessage;
    data.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (data.hIcon == nullptr) {
        data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    CopyLimited(data.szTip, ARRAYSIZE(data.szTip), L"OJfix");

    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        return false;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

void ShutdownNotifications(HWND owner)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
}

void ShowNotification(
    HWND owner,
    const std::wstring& title,
    const std::wstring& message,
    NotificationKind kind)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = kTrayIconId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = IconFlag(kind);
    data.uTimeout = 4000;
    CopyLimited(data.szInfoTitle, ARRAYSIZE(data.szInfoTitle), title);
    CopyLimited(data.szInfo, ARRAYSIZE(data.szInfo), message);

    Shell_NotifyIconW(NIM_MODIFY, &data);
}

} // namespace ojfix

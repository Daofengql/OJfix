#define WIN32_LEAN_AND_MEAN

#include "Clipboard.h"

#include <cstring>
#include <vector>

namespace ojfix {
namespace {

bool OpenClipboardWithRetry(HWND owner)
{
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (OpenClipboard(owner)) {
            return true;
        }

        Sleep(20);
    }

    return false;
}

bool ReadUnicodeClipboardText(HWND owner, std::wstring& text)
{
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return false;
    }

    if (!OpenClipboardWithRetry(owner)) {
        return false;
    }

    bool success = false;
    HANDLE clipboardData = GetClipboardData(CF_UNICODETEXT);
    if (clipboardData != nullptr) {
        const wchar_t* rawText = static_cast<const wchar_t*>(GlobalLock(clipboardData));
        if (rawText != nullptr) {
            text.assign(rawText);
            GlobalUnlock(clipboardData);
            success = true;
        }
    }

    CloseClipboard();
    return success;
}

bool ReadAnsiClipboardText(HWND owner, std::wstring& text)
{
    if (!IsClipboardFormatAvailable(CF_TEXT)) {
        return false;
    }

    if (!OpenClipboardWithRetry(owner)) {
        return false;
    }

    bool success = false;
    HANDLE clipboardData = GetClipboardData(CF_TEXT);
    if (clipboardData != nullptr) {
        const char* rawText = static_cast<const char*>(GlobalLock(clipboardData));
        if (rawText != nullptr) {
            const int requiredSize = MultiByteToWideChar(
                CP_ACP,
                0,
                rawText,
                -1,
                nullptr,
                0);

            if (requiredSize > 0) {
                std::vector<wchar_t> buffer(static_cast<size_t>(requiredSize), L'\0');
                MultiByteToWideChar(
                    CP_ACP,
                    0,
                    rawText,
                    -1,
                    buffer.data(),
                    requiredSize);

                text.assign(buffer.data());
                success = true;
            }

            GlobalUnlock(clipboardData);
        }
    }

    CloseClipboard();
    return success;
}

} // namespace

bool ReadClipboardPlainText(HWND owner, std::wstring& text)
{
    if (ReadUnicodeClipboardText(owner, text)) {
        return true;
    }

    return ReadAnsiClipboardText(owner, text);
}

bool SetClipboardUnicodeText(HWND owner, const std::wstring& text)
{
    if (!OpenClipboardWithRetry(owner)) {
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (clipboardMemory == nullptr) {
        CloseClipboard();
        return false;
    }

    void* lockedMemory = GlobalLock(clipboardMemory);
    if (lockedMemory == nullptr) {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        return false;
    }

    memcpy(lockedMemory, text.c_str(), bytes);
    GlobalUnlock(clipboardMemory);

    if (SetClipboardData(CF_UNICODETEXT, clipboardMemory) == nullptr) {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

} // namespace ojfix

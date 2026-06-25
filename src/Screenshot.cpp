#define WIN32_LEAN_AND_MEAN

#include "Screenshot.h"

#include <objidl.h>
#include <olectl.h>
#include <gdiplus.h>

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

namespace ojfix {
namespace {

struct GdiplusSession {
    ULONG_PTR token = 0;
    Gdiplus::Status status = Gdiplus::GenericError;

    GdiplusSession()
    {
        Gdiplus::GdiplusStartupInput input;
        status = Gdiplus::GdiplusStartup(&token, &input, nullptr);
    }

    ~GdiplusSession()
    {
        if (token != 0) {
            Gdiplus::GdiplusShutdown(token);
        }
    }
};

std::string Base64Encode(const unsigned char* data, size_t size)
{
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((size + 2) / 3) * 4);

    for (size_t index = 0; index < size; index += 3) {
        const uint32_t a = data[index];
        const uint32_t b = index + 1 < size ? data[index + 1] : 0;
        const uint32_t c = index + 2 < size ? data[index + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;

        output.push_back(kTable[(triple >> 18) & 0x3F]);
        output.push_back(kTable[(triple >> 12) & 0x3F]);
        output.push_back(index + 1 < size ? kTable[(triple >> 6) & 0x3F] : '=');
        output.push_back(index + 2 < size ? kTable[triple & 0x3F] : '=');
    }

    return output;
}

bool GetPngEncoderClsid(CLSID& clsid)
{
    UINT count = 0;
    UINT bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || count == 0 || bytes == 0) {
        return false;
    }

    std::vector<unsigned char> buffer(bytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok) {
        return false;
    }

    for (UINT index = 0; index < count; ++index) {
        if (std::wcscmp(encoders[index].MimeType, L"image/png") == 0) {
            clsid = encoders[index].Clsid;
            return true;
        }
    }

    return false;
}

bool EncodeBitmapToPngBase64(HBITMAP bitmapHandle, std::string& pngBase64, std::string& error)
{
    GdiplusSession gdiplus;
    if (gdiplus.status != Gdiplus::Ok) {
        error = "Failed to initialize GDI+.";
        return false;
    }

    CLSID pngClsid = {};
    if (!GetPngEncoderClsid(pngClsid)) {
        error = "PNG encoder was not found.";
        return false;
    }

    Gdiplus::Bitmap bitmap(bitmapHandle, nullptr);
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        error = "Failed to read screenshot bitmap.";
        return false;
    }

    IStream* stream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    if (FAILED(hr) || stream == nullptr) {
        error = "Failed to create PNG memory stream.";
        return false;
    }

    const Gdiplus::Status saveStatus = bitmap.Save(stream, &pngClsid, nullptr);
    if (saveStatus != Gdiplus::Ok) {
        stream->Release();
        error = "Failed to encode screenshot as PNG.";
        return false;
    }

    STATSTG stat = {};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr) || stat.cbSize.QuadPart > static_cast<ULONGLONG>(SIZE_MAX)) {
        stream->Release();
        error = "Failed to get PNG memory size.";
        return false;
    }

    HGLOBAL globalMemory = nullptr;
    hr = GetHGlobalFromStream(stream, &globalMemory);
    if (FAILED(hr) || globalMemory == nullptr) {
        stream->Release();
        error = "Failed to access PNG memory.";
        return false;
    }

    void* lockedMemory = GlobalLock(globalMemory);
    if (lockedMemory == nullptr) {
        stream->Release();
        error = "Failed to lock PNG memory.";
        return false;
    }

    pngBase64 = Base64Encode(
        static_cast<const unsigned char*>(lockedMemory),
        static_cast<size_t>(stat.cbSize.QuadPart));
    GlobalUnlock(globalMemory);
    stream->Release();
    return true;
}

} // namespace

bool CaptureVirtualScreenPngBase64(std::string& pngBase64, std::string& error)
{
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0) {
        error = "Invalid virtual screen size.";
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        error = "Failed to get screen DC. Error: " + std::to_string(GetLastError());
        return false;
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (memoryDc == nullptr) {
        error = "Failed to create memory DC. Error: " + std::to_string(GetLastError());
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (bitmap == nullptr) {
        error = "Failed to create screenshot bitmap. Error: " + std::to_string(GetLastError());
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
    if (oldObject == nullptr || oldObject == HGDI_ERROR) {
        error = "Failed to select screenshot bitmap. Error: " + std::to_string(GetLastError());
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    const BOOL copied = BitBlt(
        memoryDc,
        0,
        0,
        width,
        height,
        screenDc,
        x,
        y,
        SRCCOPY | CAPTUREBLT);

    SelectObject(memoryDc, oldObject);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied) {
        error = "Failed to copy screen pixels. Error: " + std::to_string(GetLastError());
        DeleteObject(bitmap);
        return false;
    }

    const bool success = EncodeBitmapToPngBase64(bitmap, pngBase64, error);
    DeleteObject(bitmap);
    return success;
}

} // namespace ojfix

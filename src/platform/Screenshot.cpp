#include "platform/Screenshot.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstring>

using Microsoft::WRL::ComPtr;

namespace radiance::platform
{
    namespace
    {
        BOOL CALLBACK CollectArea(HMONITOR monitor, HDC, LPRECT, LPARAM data)
        {
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(monitor, &info))
            {
                reinterpret_cast<std::vector<DisplayArea>*>(data)->push_back(
                    DisplayArea{ Narrow(info.szDevice), info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom });
            }
            return TRUE;
        }
    }

    std::optional<Screenshot> CaptureDesktop()
    {
        Screenshot shot;
        shot.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        shot.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        shot.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        shot.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (shot.width <= 0 || shot.height <= 0)
            return std::nullopt;

        HDC screen = GetDC(nullptr);
        HDC memory = CreateCompatibleDC(screen);
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = shot.width;
        bi.bmiHeader.biHeight = -shot.height;  // top row first
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        bool ok = false;
        if (bitmap && bits)
        {
            HGDIOBJ old = SelectObject(memory, bitmap);
            ok = BitBlt(memory, 0, 0, shot.width, shot.height, screen, shot.left, shot.top, SRCCOPY | CAPTUREBLT) != FALSE;
            SelectObject(memory, old);
            if (ok)
            {
                const size_t count = static_cast<size_t>(shot.width) * static_cast<size_t>(shot.height);
                shot.pixels.resize(count);
                std::memcpy(shot.pixels.data(), bits, count * sizeof(uint32_t));
                for (uint32_t& p : shot.pixels)
                    p |= 0xFF000000u;  // GDI leaves alpha at 0; a PNG would come out transparent
            }
        }
        if (bitmap)
            DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        if (!ok)
            return std::nullopt;
        return shot;
    }

    std::vector<DisplayArea> DisplayAreas()
    {
        std::vector<DisplayArea> areas;
        EnumDisplayMonitors(nullptr, nullptr, &CollectArea, reinterpret_cast<LPARAM>(&areas));
        return areas;
    }

    bool SavePng(const std::filesystem::path& file, const Screenshot& shot)
    {
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
            return false;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) ||
            FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) || FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
            FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
            FAILED(frame->SetSize(static_cast<UINT>(shot.width), static_cast<UINT>(shot.height))))
            return false;
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA)
            return false;
        const UINT stride = static_cast<UINT>(shot.width) * 4;
        const UINT size = stride * static_cast<UINT>(shot.height);
        auto* data = reinterpret_cast<BYTE*>(const_cast<uint32_t*>(shot.pixels.data()));
        return SUCCEEDED(frame->WritePixels(static_cast<UINT>(shot.height), stride, size, data)) && SUCCEEDED(frame->Commit()) &&
               SUCCEEDED(encoder->Commit());
    }

    bool CopyImage(void* ownerWindow, const Screenshot& shot)
    {
        // CF_DIB: a header and bottom-up rows, which every program that pastes understands.
        const size_t stride = static_cast<size_t>(shot.width) * 4;
        const size_t bytes = sizeof(BITMAPINFOHEADER) + stride * static_cast<size_t>(shot.height);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
            return false;
        auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(memory));
        if (!header)
        {
            GlobalFree(memory);
            return false;
        }
        *header = BITMAPINFOHEADER{};
        header->biSize = sizeof(BITMAPINFOHEADER);
        header->biWidth = shot.width;
        header->biHeight = shot.height;
        header->biPlanes = 1;
        header->biBitCount = 32;
        header->biCompression = BI_RGB;
        auto* rows = reinterpret_cast<uint8_t*>(header + 1);
        for (int y = 0; y < shot.height; ++y)
            std::memcpy(rows + stride * static_cast<size_t>(shot.height - 1 - y), shot.pixels.data() + static_cast<size_t>(y) * shot.width, stride);
        GlobalUnlock(memory);

        if (!OpenClipboard(static_cast<HWND>(ownerWindow)))
        {
            GlobalFree(memory);
            return false;
        }
        EmptyClipboard();
        const bool ok = SetClipboardData(CF_DIB, memory) != nullptr;
        CloseClipboard();
        if (!ok)
            GlobalFree(memory);  // the clipboard owns it only on success
        return ok;
    }

    std::filesystem::path ScreenshotFolder()
    {
        PWSTR pictures = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &pictures)))
            return {};
        std::filesystem::path folder = std::filesystem::path(pictures) / L"Radiance";
        CoTaskMemFree(pictures);
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        return ec ? std::filesystem::path() : folder;
    }
}

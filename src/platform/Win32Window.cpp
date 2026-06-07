#include "platform/Win32Window.h"

#include <algorithm>
#include <stdexcept>

namespace sr {

#ifdef _WIN32

namespace {

constexpr const wchar_t* windowClassName = L"SimpleRendererWindowClass";

std::wstring widen(const char* text)
{
    std::wstring result;
    while (*text != '\0') {
        result.push_back(static_cast<unsigned char>(*text));
        ++text;
    }
    return result;
}

} // namespace

Win32Window::Win32Window(void* nativeInstance, int showCommand, int width, int height, const char* title)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Window dimensions must be positive.");
    }

    HINSTANCE instance = static_cast<HINSTANCE>(nativeInstance);

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = &Win32Window::windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = windowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("Failed to register Win32 window class.");
    }

    RECT rect = { 0, 0, width, height };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE)) {
        throw std::runtime_error("Failed to calculate Win32 window size.");
    }

    const std::wstring wideTitle = widen(title ? title : "");
    hwnd_ = CreateWindowExW(
        0,
        windowClassName,
        wideTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd_) {
        throw std::runtime_error("Failed to create Win32 window.");
    }

    deviceContext_ = GetDC(hwnd_);
    if (!deviceContext_) {
        throw std::runtime_error("Failed to acquire Win32 device context.");
    }

    bitmapInfo_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo_.bmiHeader.biWidth = width;
    bitmapInfo_.bmiHeader.biHeight = -height;
    bitmapInfo_.bmiHeader.biPlanes = 1;
    bitmapInfo_.bmiHeader.biBitCount = 32;
    bitmapInfo_.bmiHeader.biCompression = BI_RGB;
    windowedPlacement_.length = sizeof(WINDOWPLACEMENT);

    ShowWindow(hwnd_, showCommand);
}

Win32Window::~Win32Window()
{
    if (hwnd_ && deviceContext_) {
        ReleaseDC(hwnd_, deviceContext_);
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool Win32Window::processMessages()
{
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return true;
}

InputState Win32Window::inputState()
{
    const bool isActive = GetForegroundWindow() == hwnd_;
    if (!isActive) {
        hasLastMousePosition_ = false;
        previousFullscreenToggleDown_ = false;
        return {};
    }

    const auto keyDown = [](int key) {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    };
    const auto keyPressed = [](int key) {
        return (GetAsyncKeyState(key) & 0x0001) != 0;
    };

    int renderModeSelection = 0;
    for (int i = 0; i < 8; ++i) {
        if (keyDown('1' + i)) {
            renderModeSelection = i + 1;
        }
    }

    const bool mouseLook = keyDown(VK_RBUTTON);
    const bool fullscreenToggleDown = keyDown(VK_F11) || ((keyDown(VK_MENU) || keyDown(VK_RMENU) || keyDown(VK_LMENU)) && keyDown(VK_RETURN));
    const bool toggleFullscreen = keyPressed(VK_F11) || (fullscreenToggleDown && !previousFullscreenToggleDown_);
    previousFullscreenToggleDown_ = fullscreenToggleDown;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;

    POINT mousePosition = {};
    if (GetCursorPos(&mousePosition)) {
        ScreenToClient(hwnd_, &mousePosition);
        if (mouseLook && hasLastMousePosition_) {
            mouseDeltaX = std::clamp(static_cast<float>(mousePosition.x - lastMousePosition_.x), -250.0f, 250.0f);
            mouseDeltaY = std::clamp(static_cast<float>(mousePosition.y - lastMousePosition_.y), -250.0f, 250.0f);
        }
        lastMousePosition_ = mousePosition;
        hasLastMousePosition_ = true;
    } else {
        hasLastMousePosition_ = false;
    }

    return {
        keyDown('W'),
        keyDown('S'),
        keyDown('A'),
        keyDown('D'),
        keyDown('E') || keyDown(VK_SPACE),
        keyDown('Q') || keyDown(VK_CONTROL),
        keyDown(VK_LEFT),
        keyDown(VK_RIGHT),
        keyDown(VK_UP),
        keyDown(VK_DOWN),
        keyDown(VK_SHIFT),
        mouseLook,
        mouseDeltaX,
        mouseDeltaY,
        renderModeSelection,
        toggleFullscreen,
        keyPressed(VK_F2),
        keyPressed(VK_F3),
    };
}

void Win32Window::setTitle(const char* title)
{
    if (hwnd_) {
        SetWindowTextW(hwnd_, widen(title ? title : "").c_str());
    }
}

bool Win32Window::clientSize(int& width, int& height) const
{
    width = 0;
    height = 0;
    if (!hwnd_) {
        return false;
    }

    RECT rect = {};
    if (!GetClientRect(hwnd_, &rect)) {
        return false;
    }

    width = std::max(0, static_cast<int>(rect.right - rect.left));
    height = std::max(0, static_cast<int>(rect.bottom - rect.top));
    return width > 0 && height > 0;
}

void Win32Window::toggleFullscreen()
{
    if (!hwnd_) {
        return;
    }

    if (!fullscreen_) {
        windowedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        windowedExStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
        windowedPlacement_.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd_, &windowedPlacement_);

        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_ & ~WS_OVERLAPPEDWINDOW);
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, windowedExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
            SetWindowPos(
                hwnd_,
                HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen_ = true;
        }
        return;
    }

    SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, windowedExStyle_);
    SetWindowPlacement(hwnd_, &windowedPlacement_);
    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    fullscreen_ = false;
}

void Win32Window::present(const Framebuffer& framebuffer)
{
    if (!hwnd_ || !deviceContext_ || framebuffer.width() <= 0 || framebuffer.height() <= 0) {
        return;
    }

    int targetWidth = 0;
    int targetHeight = 0;
    if (!clientSize(targetWidth, targetHeight)) {
        return;
    }

    bitmapInfo_.bmiHeader.biWidth = framebuffer.width();
    bitmapInfo_.bmiHeader.biHeight = -framebuffer.height();

    StretchDIBits(
        deviceContext_,
        0,
        0,
        targetWidth,
        targetHeight,
        0,
        0,
        framebuffer.width(),
        framebuffer.height(),
        framebuffer.pixels(),
        &bitmapInfo_,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK Win32Window::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

#else

Win32Window::Win32Window(void*, int, int, int, const char*)
{
    throw std::runtime_error("SimpleRenderer currently supports Windows only.");
}

Win32Window::~Win32Window() = default;

bool Win32Window::processMessages()
{
    return false;
}

InputState Win32Window::inputState()
{
    return {};
}

void Win32Window::setTitle(const char*)
{
}

bool Win32Window::clientSize(int&, int&) const
{
    return false;
}

void Win32Window::toggleFullscreen()
{
}

void Win32Window::present(const Framebuffer&)
{
}

#endif

} // namespace sr

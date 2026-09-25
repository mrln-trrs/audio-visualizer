#include "platform/window_effects.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace platform {
namespace {

// Valores de DWMWINDOWATTRIBUTE. Se definen aquí por si el SDK es anterior.
constexpr DWORD kAttrUseImmersiveDarkMode = 20;
constexpr DWORD kAttrWindowCornerPreference = 33;
constexpr DWORD kAttrSystemBackdropType = 38;
// DWM_WINDOW_CORNER_PREFERENCE
constexpr int kCornerRound = 2;
// DWM_SYSTEMBACKDROP_TYPE
constexpr int kBackdropNone = 1;
constexpr int kBackdropMica = 2;
constexpr int kBackdropAcrylic = 3;

// Número de build del sistema real, sin la compatibilidad de manifiesto de GetVersionEx.
DWORD WindowsBuildNumber() {
    typedef LONG(WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;
    auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!fn) return 0;
    RTL_OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (fn(&info) != 0) return 0;
    return info.dwBuildNumber;
}

} // namespace

bool SupportsSystemBackdrop() {
    return WindowsBuildNumber() >= 22621;
}

WindowEffectsResult ApplyWindowEffects(GLFWwindow* window, SystemBackdrop backdrop) {
    WindowEffectsResult result;
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) {
        result.message = "sin HWND";
        return result;
    }

    BOOL dark = TRUE;
    result.dark_frame = SUCCEEDED(DwmSetWindowAttribute(hwnd, kAttrUseImmersiveDarkMode, &dark, sizeof(dark)));

    int corners = kCornerRound;
    result.rounded_corners = SUCCEEDED(DwmSetWindowAttribute(hwnd, kAttrWindowCornerPreference, &corners, sizeof(corners)));

    if (backdrop == SystemBackdrop::None) {
        int none = kBackdropNone;
        DwmSetWindowAttribute(hwnd, kAttrSystemBackdropType, &none, sizeof(none));
        return result;
    }
    if (!SupportsSystemBackdrop()) {
        result.message = "los materiales del sistema requieren Windows 11 22H2 (build 22621); la ventana queda opaca";
        return result;
    }

    // El material se compone detrás de toda el área cliente.
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    int type = backdrop == SystemBackdrop::Mica ? kBackdropMica : kBackdropAcrylic;
    const HRESULT hr = DwmSetWindowAttribute(hwnd, kAttrSystemBackdropType, &type, sizeof(type));
    result.backdrop = SUCCEEDED(hr);
    if (!result.backdrop) result.message = "DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE) fallo; la ventana queda opaca";
    return result;
}

SystemEffectsPreference ReadSystemEffectsPreference() {
    SystemEffectsPreference pref;
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0)) pref.animations = animations != FALSE;

    DWORD value = 1, size = sizeof(value);
    const LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                    L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (st == ERROR_SUCCESS) pref.transparency = value != 0;
    return pref;
}

} // namespace platform

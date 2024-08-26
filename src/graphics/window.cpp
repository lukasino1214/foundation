#include "window.hpp"

#if defined(_WIN32)
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif // DWMWA_USE_IMMERSIVE_DARK_MODE
#endif // defined(_WIN32)

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>


namespace foundation {
    AppWindow::AppWindow(i32 width, i32 height, std::string_view _name)
        : window_state{std::make_unique<WindowState>()},
        name{_name},
        glfw_handle{[&]() {
                glfwInit();
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                return glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
            }()} {
        glfwSetWindowUserPointer(this->glfw_handle, window_state.get());

        glfwSetWindowCloseCallback(this->glfw_handle, [](GLFWwindow* window_ptr) {
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window_ptr));
            state->close_requested = true;
        });

        glfwSetWindowSizeCallback(this->glfw_handle, [](GLFWwindow* window_ptr, i32, i32) {
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window_ptr));
            state->resize_requested = true;
        });

        glfwSetKeyCallback(this->glfw_handle, [](GLFWwindow* window_ptr, i32 key, i32, i32 action, i32){
            if (key == -1) { return; }
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window_ptr));
            state->key_down[static_cast<u32>(key)] = action != GLFW_RELEASE;
        });

        glfwSetMouseButtonCallback(this->glfw_handle, [](GLFWwindow* window_ptr, i32 key, i32 action, i32){
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window_ptr));
            state->mouse_button_down[static_cast<u32>(key)] = action != GLFW_RELEASE;
        });

        glfwSetCursorPosCallback(this->glfw_handle, [](GLFWwindow* window_ptr, f64 x, f64 y) {
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window_ptr));
            state->cursor_change_x = static_cast<i32>(std::floor(x)) - state->old_cursor_pos_x;
            state->cursor_change_y = static_cast<i32>(std::floor(y)) - state->old_cursor_pos_y;
        });

        glfwSetWindowFocusCallback(this->glfw_handle, [](GLFWwindow *window, int focused) {
            auto *state = reinterpret_cast<WindowState *>(glfwGetWindowUserPointer(window));
            state->focused = (focused != 0); 
        });

    #if defined(_WIN32)
        {
            auto hwnd = static_cast<HWND>(glfwGetWin32Window(glfw_handle));
            BOOL value = true;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
            auto is_windows11_or_greater = []() -> bool {
                using Fn_RtlGetVersion = void(WINAPI *)(OSVERSIONINFOEX *);
                Fn_RtlGetVersion fn_RtlGetVersion = nullptr;
                auto ntdll_dll = LoadLibrary(TEXT("ntdll.dll"));
                if (ntdll_dll) { fn_RtlGetVersion = (Fn_RtlGetVersion)GetProcAddress(ntdll_dll, "RtlGetVersion"); }
                auto version_info = OSVERSIONINFOEX{};
                version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
                fn_RtlGetVersion(&version_info);
                FreeLibrary(ntdll_dll);
                return version_info.dwMajorVersion >= 10 && version_info.dwMinorVersion >= 0 && version_info.dwBuildNumber >= 22000;
            };

            if (!is_windows11_or_greater()) {
                MSG msg 
                {.hwnd = hwnd, .message = WM_NCACTIVATE, .wParam = FALSE, .lParam = 0};
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                msg.wParam = TRUE;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    #endif //_WIN32
    }

    AppWindow::~AppWindow() {
        glfwDestroyWindow(this->glfw_handle);
        glfwTerminate();
    }

    auto AppWindow::create_swapchain(daxa::Device& device) const -> daxa::Swapchain {
        auto get_native_platform = []() -> daxa::NativeWindowPlatform {
            switch (glfwGetPlatform()) {
                case GLFW_PLATFORM_WIN32:
                    return daxa::NativeWindowPlatform::WIN32_API;
                case GLFW_PLATFORM_X11:
                    return daxa::NativeWindowPlatform::XLIB_API;
                case GLFW_PLATFORM_WAYLAND:
                    return daxa::NativeWindowPlatform::WAYLAND_API;
                default:
                    return daxa::NativeWindowPlatform::UNKNOWN;
            }
        };

        auto get_native_handle = [&]() -> daxa::NativeWindowHandle {
            #if defined(_WIN32)
                return glfwGetWin32Window(glfw_handle);
            #elif defined(__linux__)
                switch (get_native_platform()) {
                    case daxa::NativeWindowPlatform::WAYLAND_API:
                        return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetWaylandWindow(glfw_handle));
                    case daxa::NativeWindowPlatform::XLIB_API:
                    default:
                        return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(glfw_handle));
                }
            #endif
        };

        return device.create_swapchain(daxa::SwapchainInfo {
            .native_window = get_native_handle(),
            .native_window_platform = get_native_platform(),
            .surface_format_selector = [](daxa::Format format) -> i32 {
                switch (format) {
                    // case daxa::Format::B8G8R8A8_UNORM: return 90;
                    // case daxa::Format::R8G8B8A8_UNORM: return 80;
                    case daxa::Format::B8G8R8A8_SRGB: return 70;
                    case daxa::Format::R8G8B8A8_SRGB: return 60;
                    default: return 0;
                }
            },
            .present_mode = daxa::PresentMode::MAILBOX,
            .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
            .name = "swapchain's" + name
        });
    }

    auto AppWindow::get_name() const -> const std::string & {
        return this->name;
    }

    auto AppWindow::is_focused() const -> bool {
        return this->window_state->focused;
    }

    auto AppWindow::key_pressed(Key key) const -> bool {
        return window_state->key_down[static_cast<u32>(key)];
    }

    auto AppWindow::key_just_pressed(Key key) const -> bool {
        return !this->window_state->key_down_old[static_cast<u32>(key)] && this->window_state->key_down[static_cast<u32>(key)];
    }

    auto AppWindow::key_just_released(Key key) const -> bool {
        return this->window_state->key_down_old[static_cast<u32>(key)] && !this->window_state->key_down[static_cast<u32>(key)];
    }

    auto AppWindow::button_pressed(Button button) const -> bool {
        return this->window_state->mouse_button_down[static_cast<u32>(button)];
    }

    auto AppWindow::button_just_pressed(Button button) const -> bool {
        return !this->window_state->mouse_button_down_old[static_cast<u32>(button)] && this->window_state->mouse_button_down[static_cast<u32>(button)];
    }

    auto AppWindow::button_just_released(Button button) const -> bool {
        return this->window_state->mouse_button_down_old[static_cast<u32>(button)] && !this->window_state->mouse_button_down[static_cast<u32>(button)];
    }

    auto AppWindow::get_cursor_x() const -> i32 {
        f64 x = {};
        f64 y = {};
        glfwGetCursorPos(this->glfw_handle, &x, &y);
        return static_cast<i32>(std::floor(x));
    }

    auto AppWindow::get_cursor_y() const -> i32 {
        f64 x = {};
        f64 y = {};
        glfwGetCursorPos(this->glfw_handle, &x, &y);
        return static_cast<i32>(std::floor(y));
    }

    auto AppWindow::get_cursor_change_x() const -> i32 {
        return this->window_state->cursor_change_x;
    }

    auto AppWindow::get_cursor_change_y() const -> i32 {
        return this->window_state->cursor_change_y;
    }

    auto AppWindow::is_cursor_over_window() const -> bool {
        f64 x = {};
        f64 y = {};
        glfwGetCursorPos(this->glfw_handle, &x, &y);
        i32 width = {};
        i32 height = {};
        glfwGetWindowSize(this->glfw_handle, &width, &height);
        return x >= 0 && x <= width && y >= 0 && y <= height;
    }

    void AppWindow::capture_cursor() {
        glfwSetInputMode(this->glfw_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void AppWindow::release_cursor() {
        glfwSetInputMode(this->glfw_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    auto AppWindow::is_cursor_captured() const -> bool {
        return glfwGetInputMode(this->glfw_handle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    }

    auto AppWindow::update() -> bool {
        this->window_state->key_down_old = this->window_state->key_down;
        this->window_state->mouse_button_down_old = this->window_state->mouse_button_down;
        this->window_state->old_cursor_pos_x = this->get_cursor_x();
        this->window_state->old_cursor_pos_y = this->get_cursor_y();
        this->window_state->cursor_change_x = {};
        this->window_state->cursor_change_y = {};

        glfwPollEvents();
        if (this->is_cursor_captured()) {
            glfwSetCursorPos(this->glfw_handle, -10000, -10000);
        }
        return this->window_state->close_requested;
    }

    void AppWindow::set_width(u32 width) const {
        i32 old_width = {};
        i32 old_height = {};
        glfwGetWindowSize(this->glfw_handle, &old_width, &old_height);
        glfwSetWindowSize(this->glfw_handle, static_cast<i32>(width), old_height);
    }

    void AppWindow::set_height(u32 height) const {
        i32 old_width = {};
        i32 old_height = {};
        glfwGetWindowSize(this->glfw_handle, &old_width, &old_height);
        glfwSetWindowSize(this->glfw_handle, old_width, static_cast<i32>(height));
    }

    auto AppWindow::get_width() const -> u32 {
        i32 width = {};
        i32 height = {};
        glfwGetWindowSize(this->glfw_handle, &width, &height);
        return static_cast<u32>(width);
    }

    auto AppWindow::get_height() const -> u32 {
        i32 width = {};
        i32 height = {};
        glfwGetWindowSize(this->glfw_handle, &width, &height);
        return static_cast<u32>(height);
    }
}

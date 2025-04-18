#pragma once

#include <pch.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace foundation {
    enum struct Key : u32 {
        Space = GLFW_KEY_SPACE,
        Apostrophe = GLFW_KEY_APOSTROPHE,
        Comma = GLFW_KEY_COMMA,
        Minus = GLFW_KEY_MINUS,
        Period = GLFW_KEY_PERIOD,
        Slash = GLFW_KEY_SLASH,
        Key_0 = GLFW_KEY_0,
        Key_1 = GLFW_KEY_1,
        Key_2 = GLFW_KEY_2,
        Key_3 = GLFW_KEY_3,
        Key_4 = GLFW_KEY_4,
        Key_5 = GLFW_KEY_5,
        Key_6 = GLFW_KEY_6,
        Key_7 = GLFW_KEY_7,
        Key_8 = GLFW_KEY_8,
        Key_9 = GLFW_KEY_9,
        Semicolon = GLFW_KEY_SEMICOLON,
        Equal = GLFW_KEY_EQUAL,
        A = GLFW_KEY_A,
        B = GLFW_KEY_B,
        C = GLFW_KEY_C,
        D = GLFW_KEY_D,
        E = GLFW_KEY_E,
        F = GLFW_KEY_F,
        G = GLFW_KEY_G,
        H = GLFW_KEY_H,
        I = GLFW_KEY_I,
        J = GLFW_KEY_J,
        K = GLFW_KEY_K,
        L = GLFW_KEY_L,
        M = GLFW_KEY_M,
        N = GLFW_KEY_N,
        O = GLFW_KEY_O,
        P = GLFW_KEY_P,
        Q = GLFW_KEY_Q,
        R = GLFW_KEY_R,
        S = GLFW_KEY_S,
        T = GLFW_KEY_T,
        U = GLFW_KEY_U,
        V = GLFW_KEY_V,
        W = GLFW_KEY_W,
        X = GLFW_KEY_X,
        Y = GLFW_KEY_Y,
        Z = GLFW_KEY_Z,
        LeftBracket = GLFW_KEY_LEFT_BRACKET,
        Backslash = GLFW_KEY_BACKSLASH,
        RightBracket = GLFW_KEY_RIGHT_BRACKET,
        GraveAccent = GLFW_KEY_GRAVE_ACCENT,
        Escape = GLFW_KEY_ESCAPE,
        Enter = GLFW_KEY_ENTER,
        Tab = GLFW_KEY_TAB,
        Backspace = GLFW_KEY_BACKSPACE,
        Insert = GLFW_KEY_INSERT,
        Delete = GLFW_KEY_DELETE,
        Right = GLFW_KEY_RIGHT,
        Left = GLFW_KEY_LEFT,
        Down = GLFW_KEY_DOWN,
        Up = GLFW_KEY_UP,
        PageUp = GLFW_KEY_PAGE_UP,
        PageDown = GLFW_KEY_PAGE_DOWN,
        Home = GLFW_KEY_HOME,
        End = GLFW_KEY_END,
        CapsLock = GLFW_KEY_CAPS_LOCK,
        ScrollLock = GLFW_KEY_SCROLL_LOCK,
        NumLock = GLFW_KEY_NUM_LOCK,
        PrintScreen = GLFW_KEY_PRINT_SCREEN,
        Pause = GLFW_KEY_PAUSE,
        F1 = GLFW_KEY_F1,
        F2 = GLFW_KEY_F2,
        F3 = GLFW_KEY_F3,
        F4 = GLFW_KEY_F4,
        F5 = GLFW_KEY_F5,
        F6 = GLFW_KEY_F6,
        F7 = GLFW_KEY_F7,
        F8 = GLFW_KEY_F8,
        F9 = GLFW_KEY_F9,
        F10 = GLFW_KEY_F10,
        F11 = GLFW_KEY_F11,
        F12 = GLFW_KEY_F12,
        F13 = GLFW_KEY_F13,
        F14 = GLFW_KEY_F14,
        F15 = GLFW_KEY_F15,
        F16 = GLFW_KEY_F16,
        F17 = GLFW_KEY_F17,
        F18 = GLFW_KEY_F18,
        F19 = GLFW_KEY_F19,
        F20 = GLFW_KEY_F20,
        F21 = GLFW_KEY_F21,
        F22 = GLFW_KEY_F22,
        F23 = GLFW_KEY_F23,
        F24 = GLFW_KEY_F24,
        F25 = GLFW_KEY_F25,
        KP_0 = GLFW_KEY_KP_0,
        KP_1 = GLFW_KEY_KP_1,
        KP_2 = GLFW_KEY_KP_2,
        KP_3 = GLFW_KEY_KP_3,
        KP_4 = GLFW_KEY_KP_4,
        KP_5 = GLFW_KEY_KP_5,
        KP_6 = GLFW_KEY_KP_6,
        KP_7 = GLFW_KEY_KP_7,
        KP_8 = GLFW_KEY_KP_8,
        KP_9 = GLFW_KEY_KP_9,
        KPDecimal = GLFW_KEY_KP_DECIMAL,
        KPDivide = GLFW_KEY_KP_DIVIDE,
        KPMultiply = GLFW_KEY_KP_MULTIPLY,
        KPSubtract = GLFW_KEY_KP_SUBTRACT,
        KPAdd = GLFW_KEY_KP_ADD,
        KPEnter = GLFW_KEY_KP_ENTER,
        KPEqual = GLFW_KEY_KP_EQUAL,
        LeftShift = GLFW_KEY_LEFT_SHIFT,
        LeftControl = GLFW_KEY_LEFT_CONTROL,
        LeftAlt = GLFW_KEY_LEFT_ALT,
        LeftSuper = GLFW_KEY_LEFT_SUPER,
        RightShift = GLFW_KEY_RIGHT_SHIFT,
        RightControl = GLFW_KEY_RIGHT_CONTROL,
        RightAlt = GLFW_KEY_RIGHT_ALT,
        RightSuper = GLFW_KEY_RIGHT_SUPER,
        Menu = GLFW_KEY_MENU,
    };

    struct WindowState {
        bool close_requested = {};
        bool resize_requested = {};
        bool focused = true;
        std::array<bool, 5> mouse_button_down_old = {};
        std::array<bool, 5> mouse_button_down = {};
        std::array<bool, 512> key_down = {};
        std::array<bool, 512> key_down_old = {};
        i32 old_cursor_pos_x = {};
        i32 old_cursor_pos_y = {};
        i32 cursor_change_x = {};
        i32 cursor_change_y = {};
    };

    using Button = i32;

    struct AppWindow {
        AppWindow(i32 width, i32 height, std::string_view _name);
        ~AppWindow();

        auto create_swapchain(daxa::Device& device) const -> daxa::Swapchain;

        auto update() const -> bool;

        [[nodiscard]] auto key_pressed(Key key) const -> bool;
        [[nodiscard]] auto key_just_pressed(Key key) const -> bool;
        [[nodiscard]] auto key_just_released(Key key) const -> bool;

        [[nodiscard]] auto button_pressed(Button button) const -> bool;
        [[nodiscard]] auto button_just_pressed(Button button) const -> bool;
        [[nodiscard]] auto button_just_released(Button button) const -> bool;

        [[nodiscard]] auto get_cursor_x() const -> i32;
        [[nodiscard]] auto get_cursor_y() const -> i32;
        [[nodiscard]] auto get_cursor_change_x() const -> i32;
        [[nodiscard]] auto get_cursor_change_y() const -> i32;
        [[nodiscard]] auto is_cursor_over_window() const -> bool;
        void capture_cursor() const;
        void release_cursor() const;
        [[nodiscard]] auto is_cursor_captured() const -> bool;

        [[nodiscard]] auto is_focused() const -> bool;

        void set_width(u32 width) const;
        void set_height(u32 height) const;
        [[nodiscard]] auto get_width() const -> u32;
        [[nodiscard]] auto get_height() const -> u32;

        void set_name(std::string name);
        [[nodiscard]] auto get_name() const -> std::string const &;

        std::unique_ptr<WindowState> window_state = {};
        u32 glfw_window_id = {};
        bool cursor_captured = {};
        std::string name = {};
        GLFWwindow *glfw_handle = {};
        i32 cursor_pos_change_x = {};
        i32 cursor_pos_change_y = {};
    };
}
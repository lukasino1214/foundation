#include "scripting_engine.hpp"
#include <ecs/entity.hpp>
#include <utils/file_io.hpp>
#include <graphics/window.hpp>

namespace foundation {
    void ScriptingEngine::init_script(ScriptComponent* script, Entity entity, const std::filesystem::path& path) {
        PROFILE_SCOPE;
        auto lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base);

        lua->new_enum("Key",
            "Space", Key::Space,
            "Apostrophe", Key::Apostrophe,
            "Comma", Key::Comma,
            "Minus", Key::Minus,
            "Period", Key::Period,
            "Slash", Key::Slash,
            "Key_0", Key::Key_0,
            "Key_1", Key::Key_1,
            "Key_2", Key::Key_2,
            "Key_3", Key::Key_3,
            "Key_4", Key::Key_4,
            "Key_5", Key::Key_5,
            "Key_6", Key::Key_6,
            "Key_7", Key::Key_7,
            "Key_8", Key::Key_8,
            "Key_9", Key::Key_9,
            "Semicolon", Key::Semicolon,
            "Equal", Key::Equal,
            "A", Key::A,
            "B", Key::B,
            "C", Key::C,
            "D", Key::D,
            "E", Key::E,
            "F", Key::F,
            "G", Key::G,
            "H", Key::H,
            "I", Key::I,
            "J", Key::J,
            "K", Key::K,
            "L", Key::L,
            "M", Key::M,
            "N", Key::N,
            "O", Key::O,
            "P", Key::P,
            "Q", Key::Q,
            "R", Key::R,
            "S", Key::S,
            "T", Key::T,
            "U", Key::U,
            "V", Key::V,
            "W", Key::W,
            "X", Key::X,
            "Y", Key::Y,
            "Z", Key::Z,
            "LeftBracket", Key::LeftBracket,
            "Backslash", Key::Backslash,
            "RightBracket", Key::RightBracket,
            "GraveAccent", Key::GraveAccent,
            "Escape", Key::Escape,
            "Enter", Key::Enter,
            "Tab", Key::Tab,
            "Backspace", Key::Backspace,
            "Insert", Key::Insert,
            "Delete", Key::Delete,
            "Right", Key::Right,
            "Left", Key::Left,
            "Down", Key::Down,
            "Up", Key::Up,
            "PageUp", Key::PageUp,
            "PageDown", Key::PageDown,
            "Home", Key::Home,
            "End", Key::End,
            "CapsLock", Key::CapsLock,
            "ScrollLock", Key::ScrollLock,
            "NumLock", Key::NumLock,
            "PrintScreen", Key::PrintScreen,
            "Pause", Key::Pause,
            "F1", Key::F1,
            "F2", Key::F2,
            "F3", Key::F3,
            "F4", Key::F4,
            "F5", Key::F5,
            "F6", Key::F6,
            "F7", Key::F7,
            "F8", Key::F8,
            "F9", Key::F9,
            "F10", Key::F10,
            "F11", Key::F11,
            "F12", Key::F12,
            "F13", Key::F13,
            "F14", Key::F14,
            "F15", Key::F15,
            "F16", Key::F16,
            "F17", Key::F17,
            "F18", Key::F18,
            "F19", Key::F19,
            "F20", Key::F20,
            "F21", Key::F21,
            "F22", Key::F22,
            "F23", Key::F23,
            "F24", Key::F24,
            "F25", Key::F25,
            "KP_0", Key::KP_0,
            "KP_1", Key::KP_1,
            "KP_2", Key::KP_2,
            "KP_3", Key::KP_3,
            "KP_4", Key::KP_4,
            "KP_5", Key::KP_5,
            "KP_6", Key::KP_6,
            "KP_7", Key::KP_7,
            "KP_8", Key::KP_8,
            "KP_9", Key::KP_9,
            "KPDecimal", Key::KPDecimal,
            "KPDivide", Key::KPDivide,
            "KPMultiply", Key::KPMultiply,
            "KPSubtract", Key::KPSubtract,
            "KPAdd", Key::KPAdd,
            "KPEnter", Key::KPEnter,
            "KPEqual", Key::KPEqual,
            "LeftShift", Key::LeftShift,
            "LeftControl", Key::LeftControl,
            "LeftAlt", Key::LeftAlt,
            "LeftSuper", Key::LeftSuper,
            "RightShift", Key::RightShift,
            "RightControl", Key::RightControl,
            "RightAlt", Key::RightAlt,
            "RightSuper", Key::RightSuper,
            "Menu", Key::Menu
        );

        lua->new_usertype<AppWindow>("Window",
            "key_pressed", &AppWindow::key_pressed,
            "key_just_pressed", &AppWindow::key_just_pressed,
            "key_just_released", &AppWindow::key_just_released,
            "button_pressed", &AppWindow::button_pressed,
            "button_just_pressed", &AppWindow::button_just_pressed,
            "button_just_released", &AppWindow::button_just_released,
            "get_cursor_x", &AppWindow::get_cursor_x,
            "get_cursor_y", &AppWindow::get_cursor_y,
            "get_cursor_change_x", &AppWindow::get_cursor_change_x,
            "get_cursor_change_y", &AppWindow::get_cursor_change_y,
            "is_cursor_over_window", &AppWindow::is_cursor_over_window,
            "capture_cursor", &AppWindow::capture_cursor,
            "release_cursor", &AppWindow::release_cursor,
            "is_cursor_captured", &AppWindow::is_cursor_captured,
            "is_focused", &AppWindow::is_focused,
            "set_width", &AppWindow::set_width,
            "set_height", &AppWindow::set_height,
            "get_width", &AppWindow::get_width,
            "get_height", &AppWindow::get_height
        );

        lua->new_usertype<glm::vec3>("vec3", 
            sol::constructors<glm::vec3(), glm::vec3(f32), glm::vec3(f32, f32, f32)>(),
            "x", &glm::vec3::x, 
            "y", &glm::vec3::y, 
            "z", &glm::vec3::z,
            sol::meta_function::addition, sol::overload(
                s_cast<glm::vec3 (*)(const glm::vec3&, const glm::vec3&)>(glm::operator+),
                s_cast<glm::vec3 (*)(const glm::vec3&, f32)>(glm::operator+),
                s_cast<glm::vec3 (*)(f32, const glm::vec3&)>(glm::operator+)
            ),
            sol::meta_function::subtraction, sol::overload(
                s_cast<glm::vec3 (*)(const glm::vec3&, const glm::vec3&)>(glm::operator-),
                s_cast<glm::vec3 (*)(const glm::vec3&, f32)>(glm::operator-),
                s_cast<glm::vec3 (*)(f32, const glm::vec3&)>(glm::operator-)
            ),
            sol::meta_function::multiplication, sol::overload(
                s_cast<glm::vec3 (*)(const glm::vec3&, const glm::vec3&)>(glm::operator*),
                s_cast<glm::vec3 (*)(const glm::vec3&, f32)>(glm::operator*),
                s_cast<glm::vec3 (*)(f32, const glm::vec3&)>(glm::operator*)
            ),
            sol::meta_function::division, sol::overload(
                s_cast<glm::vec3 (*)(const glm::vec3&, const glm::vec3&)>(glm::operator/),
                    s_cast<glm::vec3 (*)(const glm::vec3&, f32)>(glm::operator/),
                    s_cast<glm::vec3 (*)(f32, const glm::vec3&)>(glm::operator/)
            ),
            sol::meta_function::equal_to, [](const glm::vec3& a, const glm::vec3& b) -> bool { return a == b; }
        );

        lua->set_function("normalize", sol::resolve<glm::vec3(const glm::vec3&)>(glm::normalize));
        lua->set_function("length", sol::resolve<f32(const glm::vec3&)>(glm::length));

        lua->new_usertype<Entity>("Entity", 
            "set_local_position", &Entity::set_local_position,
            "set_local_rotation", &Entity::set_local_rotation,
            "set_local_scale", &Entity::set_local_scale,
            "get_local_position", &Entity::get_local_position,
            "get_local_rotation", &Entity::get_local_rotation,
            "get_local_scale", &Entity::get_local_scale,
            "get_global_position", &Entity::get_global_position,
            "get_global_rotation", &Entity::get_global_rotation,
            "get_global_scale", &Entity::get_global_scale
        );

        lua->set("entity", entity);
        lua->set("window", window);
        auto result = lua->safe_script(read_file_to_string(path), sol::script_pass_on_error);
        if(!result.valid()) {
            std::println("failed to load script!");
            sol::error sol_error = result; // making MSVC happy
            std::println("{}", sol_error.what());
            return;
        }
        script->lua = std::move(lua);
        (*script->lua)["setup"]();
    }
}

#pragma once

#include <graphics/window.hpp>

namespace foundation {
    struct ScriptComponent;
    struct Entity;
    
    struct ScriptingEngine {
        AppWindow* window = {};
        void init_script(ScriptComponent* script, Entity entity, const std::filesystem::path& path);
    };
}
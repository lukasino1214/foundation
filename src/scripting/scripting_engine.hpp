#pragma once

namespace foundation {
    struct ScriptComponent;
    struct Entity;
    struct AppWindow;
    
    struct ScriptingEngine {
        AppWindow* window = {};
        void init_script(ScriptComponent* script, Entity entity, const std::filesystem::path& path);
    };
}
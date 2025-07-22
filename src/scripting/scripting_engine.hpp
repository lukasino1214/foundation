#pragma once

namespace foundation {
    struct ScriptComponent;
    struct Entity;
    struct NativeWindow;
    
    struct ScriptingEngine {
        NativeWindow* window = {};
        void init_script(ScriptComponent* script, Entity entity, const std::filesystem::path& path);
    };
}
#pragma once

namespace foundation {
    struct ScriptComponent;
    struct Entity;
    struct NativeWIndow;
    
    struct ScriptingEngine {
        NativeWIndow* window = {};
        void init_script(ScriptComponent* script, Entity entity, const std::filesystem::path& path);
    };
}
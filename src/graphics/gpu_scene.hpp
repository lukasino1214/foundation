#pragma once

namespace foundation {
    struct Scene;
    struct AssetManager;

    struct GPUScene {
        GPUScene(Scene* _scene, AssetManager* _asset_manager);
        ~GPUScene();

        void update();

        Scene* scene = {};
        AssetManager* asset_manager = {};
    };
}
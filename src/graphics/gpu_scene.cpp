#include "gpu_scene.hpp"

namespace foundation {
    GPUScene::GPUScene(Scene* _scene, AssetManager* _asset_manager) : scene{_scene}, asset_manager{_asset_manager} {}
    GPUScene::~GPUScene() = default;

    void GPUScene::update() {
        PROFILE_SCOPE;
    }
}
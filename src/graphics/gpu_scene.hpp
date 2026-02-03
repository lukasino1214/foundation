#pragma once

#include <asset/asset_processor.hpp>

namespace foundation {
    struct GPUContext;
    struct Scene;
    struct AssetManager;

    struct GPUScene {
        GPUScene(GPUContext* _gpu_context, Scene* _scene, AssetManager* _asset_manager);
        ~GPUScene();

        void update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos);

        GPUContext* gpu_context = {};
        Scene* scene = {};
        AssetManager* asset_manager = {};

        daxa::TaskTlas gpu_tlas = {};
    };
}
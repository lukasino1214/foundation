#pragma once

#include "asset_manifests.hpp"
#include "asset_processor.hpp"
#include <ecs/entity.hpp>

namespace foundation {
    struct GPUContext;
    struct ThreadPool;

    struct AssetManager {
        explicit AssetManager(GPUContext* _gpu_context, AssetProcessor* _asset_processor, ThreadPool* _thread_pool, Scene* _scene);
        ~AssetManager();

        void load_asset(const std::filesystem::path& file_path, Entity root_entity);

        void unload_asset(const std::filesystem::path& file_path);
        void unload_asset(u32 asset_index);

        void update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos);

        GPUContext* gpu_context = {};
        AssetProcessor* asset_processor = {};
        ThreadPool* thread_pool = {};
        Scene* scene = {};

        std::vector<u32> dirty_materials = {};

        ankerl::unordered_dense::map<std::filesystem::path, u32> asset_lookup = {};
        std::vector<AssetManifest> asset_manifests = {};
        std::vector<TextureManifest> texture_manifests = {};
        std::vector<MaterialManifest> material_manifests = {};
        std::vector<MeshManifest> mesh_manifests = {};
        std::vector<MeshGroupManifest> mesh_group_manifests = {};
        std::vector<AnimationManifest> animation_manifests = {};
        std::vector<SkeletonManifest> skeleton_manifests = {};

        daxa::TaskBuffer gpu_materials = {};
    };
}
#pragma once

#include "asset_manifests.hpp"

namespace foundation {
    struct GPUContext;
    struct Asset;

    struct LoadMeshInfo {
        Asset* asset;
        std::filesystem::path asset_path = {};

        u32 asset_manifest_index = {};
        u32 mesh_manifest_index = {};

        u32 asset_local_mesh_group_index = {};
        u32 asset_local_mesh_index = {};

        bool has_alpha = false;
    };

    struct MeshUploadInfo {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};

        daxa::BufferId staging_index_buffer = {};
        daxa::BufferId index_buffer = {};

        daxa::BlasId blas = {};

        MeshGeometryData mesh_geometry_data = {};

        u32 asset_manifest_index = {};
        u32 mesh_manifest_index = {};

        bool has_alpha = false;
    };

    struct LoadTextureInfo {
        Asset* asset;
        std::filesystem::path asset_path = {};
        bool load_as_srgb = {};
        TextureType texture_type = TextureType::None;

        u32 asset_manifest_index = {};
        u32 texture_manifest_index = {};
    };

    struct TextureUploadInfo {
        daxa::BufferId staging_image_buffer = {};
        daxa::ImageId image = {};
        daxa::SamplerId sampler = {};

        std::vector<std::pair<u32, u32>> mipmaps = {};

        u32 asset_manifest_index = {};
        u32 texture_manifest_index = {};
    };

    struct ResourceUploadInfos {
        std::vector<MeshUploadInfo> meshes = {};
        std::vector<TextureUploadInfo> textures = {};
    };

    struct AssetProcessor {
        explicit AssetProcessor(GPUContext* _gpu_context);
        ~AssetProcessor();

        void process_mesh_load(const LoadMeshInfo& info);
        void process_texture_load(const LoadTextureInfo& info);

        auto upload_resources(daxa::CommandRecorder& cmd_recorder) -> ResourceUploadInfos;

      private:
        GPUContext* gpu_context = {};

        std::unique_ptr<std::mutex> mesh_upload_mutex = std::make_unique<std::mutex>();
        std::unique_ptr<std::mutex> texture_upload_mutex = std::make_unique<std::mutex>();

        std::vector<MeshUploadInfo> mesh_upload_queue = {};
        std::vector<TextureUploadInfo> texture_upload_queue = {};
    };
} // namespace foundation
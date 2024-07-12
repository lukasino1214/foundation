#pragma once

#include "common/thread_pool.hpp"
#include "pch.hpp"
#include "graphics/context.hpp"
#include "asset_processor.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include "ecs/entity.hpp"
#include "ecs/scene.hpp"

namespace Shaper {
    struct LoadManifestInfo {
        Entity parent;
        std::filesystem::path path;
        Scene* scene;
        std::unique_ptr<ThreadPool> & thread_pool;
        std::unique_ptr<AssetProcessor> & asset_processor;
    };

    struct GltfAssetManifestEntry {
        std::filesystem::path path = {};
        std::unique_ptr<fastgltf::Asset> gltf_asset = {};
        u32 texture_manifest_offset = {};
        u32 material_manifest_offset = {};
        u32 mesh_manifest_offset = {};
        u32 mesh_group_manifest_offset = {};
        Entity parent;
    };

    struct MeshManifestEntry {
        struct TraditionalRenderInfo {
            daxa::BufferId vertex_buffer = {};
            daxa::BufferId index_buffer = {};
            // daxa::BufferId material_buffer = {};
            u32 material_manifest_index = {};
            u32 vertex_count = {};
            u32 index_count = {};
        };

        struct VirtualGeometryRenderInfo {
            daxa::BufferId mesh_buffer = {};
            u32 material_manifest_index = {};
        };

        u32 gltf_asset_manifest_index = {};
        u32 asset_local_mesh_index = {};
        u32 asset_local_primitive_index = {};
        std::optional<TraditionalRenderInfo> traditional_render_info = {};
        std::optional<VirtualGeometryRenderInfo> virtual_geometry_render_info = {};
    };

    struct MeshGroupManifestEntry {
        u32 mesh_manifest_indices_offset = {};
        u32 mesh_count = {};
        u32 gltf_asset_manifest_index = {};
        u32 asset_local_index = {};
        std::string name = {};
    };

    struct TextureManifestEntry {
        struct MaterialManifestIndex {
            bool albedo = {};
            bool normal = {};
            bool roughness_metalness = {};
            bool emissive = {};
            u32 material_manifest_index = {};
        };
        u32 gltf_asset_manifest_index = {};
        u32 asset_local_index = {};
        std::vector<MaterialManifestIndex> material_manifest_indices = {};
        daxa::ImageId image_id = {};
        daxa::SamplerId sampler_id = {};
        std::string name = {};
    };

    struct MaterialManifestEntry {
        struct TextureInfo {
            u32 texture_manifest_index;
            u32 sampler_index;
        };
        std::optional<TextureInfo> albedo_info = {};
        std::optional<TextureInfo> normal_info = {};
        std::optional<TextureInfo> roughness_metalness_info = {};
        std::optional<TextureInfo> emissive_info = {};
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor;
        u32 alpha_mode;
        f32 alpha_cutoff;
        bool double_sided;
        u32 gltf_asset_manifest_index = {};
        u32 asset_local_index = {};
        // daxa::BufferId material_buffer = {};
        u32 material_manifest_index = {};
        std::string name = {};
    };

    struct AssetManager {
        AssetManager(Context* _context, Scene* _scene);
        ~AssetManager();

        void load_model(LoadManifestInfo& info);
        void already_loaded_model(LoadManifestInfo& info, const GltfAssetManifestEntry& asset_manifest);

        struct RecordManifestUpdateInfo {
            std::span<const MeshUploadInfo> uploaded_meshes = {};
            std::span<const TextureUploadInfo> uploaded_textures = {};
        };

        auto record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList;

        std::vector<GltfAssetManifestEntry> gltf_asset_manifest_entries = {};
        std::vector<TextureManifestEntry> material_texture_manifest_entries = {};
        std::vector<MaterialManifestEntry> material_manifest_entries = {};
        std::vector<MeshManifestEntry> mesh_manifest_entries = {};
        std::vector<MeshGroupManifestEntry> mesh_group_manifest_entries = {};

        Context* context;
        Scene* scene;
        daxa::TaskBuffer gpu_scene_data = {};
        daxa::TaskBuffer gpu_meshes = {};
        daxa::TaskBuffer gpu_materials = {};
        daxa::TaskBuffer gpu_transforms = {};
    };
}
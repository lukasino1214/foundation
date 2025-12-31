#pragma once

#include "common/thread_pool.hpp"
#include "pch.hpp"
#include "graphics/context.hpp"
#include "asset_processor.hpp"

#include "ecs/entity.hpp"
#include "ecs/scene.hpp"
#include "ecs/gpu_scene.hpp"

namespace foundation {
    struct LoadManifestInfo {
        Entity parent;
        std::filesystem::path path;
    };

    struct AssetManifestEntry {
        std::filesystem::path path = {};
        u32 texture_manifest_offset = {};
        u32 material_manifest_offset = {};
        u32 mesh_manifest_offset = {};
        u32 mesh_group_manifest_offset = {};
        Entity parent;
        std::unique_ptr<BinaryAssetInfo> asset = {};
        BinaryModelHeader header = {};
    };

    struct MeshManifestEntry {
        struct VirtualGeometryRenderInfo {
            MeshGeometryData mesh_geometry_data = {};
            u32 material_manifest_index = {};
        };

        u32 asset_manifest_index = {};
        u32 asset_local_mesh_index = {};
        u32 asset_local_primitive_index = {};
        u8 unload_delay = {};
        bool loading = true;
        VirtualGeometryRenderInfo geometry_info = {};
    };

    struct MeshGroupManifestEntry {
        u32 mesh_manifest_indices_offset = {};
        u32 mesh_count = {};
        u32 asset_manifest_index = {};
        u32 asset_local_index = {};
        std::string name = {};
    };

    struct TextureManifestEntry {
        struct MaterialManifestIndex {
            MaterialType material_type = MaterialType::None;
            u32 material_manifest_index = {};
        };
        u32 asset_manifest_index = {};
        u32 asset_local_index = {};
        std::vector<MaterialManifestIndex> material_manifest_indices = {};
        daxa::ImageId image_id = {};
        daxa::SamplerId sampler_id = {};
        u32 current_resolution = {};
        u32 max_resolution = {};
        u8 unload_delay = {};
        bool loading = true;
        std::string name = {};
    };

    struct MaterialManifestEntry {
        struct TextureInfo {
            u32 texture_manifest_index;
            u32 sampler_index;
        };
        std::optional<TextureInfo> albedo_info = {};
        std::optional<TextureInfo> alpha_mask_info = {};
        std::optional<TextureInfo> normal_info = {};
        std::optional<TextureInfo> roughness_info = {};
        std::optional<TextureInfo> metalness_info = {};
        std::optional<TextureInfo> emissive_info = {};
        f32vec4 albedo_factor;
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor;
        BinaryAlphaMode alpha_mode;
        f32 alpha_cutoff;
        bool double_sided;
        u32 asset_manifest_index = {};
        u32 asset_local_index = {};
        u32 material_manifest_index = {};
        std::string name = {};
    };

    struct AssetManager {
        AssetManager(Context* _context, Scene* _scene, ThreadPool* _thread_pool, AssetProcessor* _asset_processor);
        ~AssetManager();

        void load_model(LoadManifestInfo& info);

        struct RecordManifestUpdateInfo {
            std::span<const MeshUploadInfo> uploaded_meshes = {};
            std::span<const TextureUploadInfo> uploaded_textures = {};
        };

        struct RecordedManifestUpdateInfo {
            daxa::ExecutableCommandList command_list = {};
            std::vector<UpdateMeshGroup> update_mesh_groups = {};
            std::vector<UpdateMesh> update_meshes = {};
        };

        void stream_textures();
        void stream_meshes();
        auto record_manifest_update(const RecordManifestUpdateInfo& info) -> RecordedManifestUpdateInfo;

        Context* context;
        Scene* scene;
        ThreadPool* thread_pool;
        AssetProcessor* asset_processor;

        std::vector<AssetManifestEntry> asset_manifest_entries = {};
        std::vector<TextureManifestEntry> texture_manifest_entries = {};
        std::vector<MaterialManifestEntry> material_manifest_entries = {};
        std::vector<MeshManifestEntry> mesh_manifest_entries = {};
        std::vector<MeshGroupManifestEntry> mesh_group_manifest_entries = {};
        
        std::vector<u32> dirty_materials = {};
        std::vector<UpdateMeshGroup> update_mesh_groups = {};
        std::vector<UpdateMesh> update_meshes = {};

        std::vector<u32> readback_material = {};
        std::vector<u32> texture_sizes = {};
        std::vector<u32> readback_mesh = {};
        
        daxa::TaskBuffer gpu_materials = {};
        daxa::TaskBuffer gpu_readback_material_gpu = {};
        daxa::TaskBuffer gpu_readback_material_cpu = {};
        daxa::TaskBuffer gpu_readback_mesh_gpu = {};
        daxa::TaskBuffer gpu_readback_mesh_cpu = {};
    };
}
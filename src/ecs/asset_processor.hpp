#pragma once
#include "pch.hpp"
#include <mutex>
#include "graphics/context.hpp"
#include <fastgltf/types.hpp>

namespace foundation {
    struct LoadMeshInfo {
        std::filesystem::path asset_path = {};
        fastgltf::Asset * asset;
        u32 gltf_mesh_index = {};
        u32 gltf_primitive_index = {};
        u32 material_manifest_offset = {};
        u32 manifest_index = {};
    };

    struct ProcessedMeshInfo {
        AABB mesh_aabb = {};
        std::vector<f32vec3> positions = {};
        std::vector<u32> normals = {};
        std::vector<u32> uvs = {};
        std::vector<Meshlet> meshlets = {};
        std::vector<BoundingSphere> bounding_spheres = {};
        std::vector<AABB> aabbs = {};
        std::vector<u8> micro_indices = {};
        std::vector<u32> indirect_vertices = {};
    };

    struct MeshBuffers {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};
        Mesh mesh = {};
    };

    struct MeshUploadInfo {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};
        Mesh mesh = {};

        u32 manifest_index = {};
        u32 material_manifest_offset = {};

        u32 meshlet_count = {};
        u32 triangle_count = {};
        u32 vertex_count = {};
    };

    struct LoadTextureInfo {
        std::filesystem::path asset_path = {};
        fastgltf::Asset * asset;
        u32 gltf_texture_index = {};
        u32 texture_manifest_index = {};
        bool load_as_srgb = {};
    };

    struct TextureUploadInfo {
        daxa::BufferId staging_buffer = {};
        daxa::ImageId dst_image = {};
        daxa::SamplerId sampler = {};
        u32 manifest_index = {};
    };

    struct RecordCommands {
        daxa::ExecutableCommandList upload_commands = {};
        std::vector<MeshUploadInfo> uploaded_meshes = {};
        std::vector<TextureUploadInfo> uploaded_textures = {};
    };

    struct AssetProcessor {
        AssetProcessor(Context* _context);
        ~AssetProcessor();

        void load_mesh(const LoadMeshInfo& info);
        void load_texture(const LoadTextureInfo& info);

        auto record_gpu_load_processing_commands() -> RecordCommands;

        static auto process_mesh(const LoadMeshInfo& info) -> ProcessedMeshInfo;
        static auto create_mesh_buffers(Context* context, const LoadMeshInfo& info, const ProcessedMeshInfo& processed_info) -> MeshBuffers;

        Context* context;

        std::vector<MeshUploadInfo> mesh_upload_queue = {};
        std::vector<TextureUploadInfo> texture_upload_queue = {};

        std::unique_ptr<std::mutex> mesh_upload_mutex = std::make_unique<std::mutex>();
        std::unique_ptr<std::mutex> texture_upload_mutex = std::make_unique<std::mutex>();
    };
}
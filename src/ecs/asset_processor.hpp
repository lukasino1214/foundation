#pragma once
#include "pch.hpp"
#include <mutex>
#include "graphics/context.hpp"
#include <fastgltf/types.hpp>
#include <ecs/binary_assets.hpp>

namespace foundation {
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

        static void serialize(ByteWriter& writer, const ProcessedMeshInfo& value) {
            writer.write(value.mesh_aabb);
            writer.write(value.positions);
            writer.write(value.normals);
            writer.write(value.uvs);
            writer.write(value.meshlets);
            writer.write(value.bounding_spheres);
            writer.write(value.aabbs);
            writer.write(value.micro_indices);
            writer.write(value.indirect_vertices);
        }

        static auto deserialize(ByteReader& reader) -> ProcessedMeshInfo { 
            ProcessedMeshInfo value = {};
            reader.read(value.mesh_aabb);
            reader.read(value.positions);
            reader.read(value.normals);
            reader.read(value.uvs);
            reader.read(value.meshlets);
            reader.read(value.bounding_spheres);
            reader.read(value.aabbs);
            reader.read(value.micro_indices);
            reader.read(value.indirect_vertices);
            return value;    
        }
    };

    struct ProcessMeshInfo {
        fastgltf::Asset * asset;
        u32 gltf_mesh_index = {};
        u32 gltf_primitive_index = {};
    };

    struct LoadMeshInfo {
        std::filesystem::path asset_path = {};
        BinaryAssetInfo* asset = {};
        u32 mesh_group_index = {};
        u32 mesh_index = {};
        u32 material_manifest_offset = {};
        u32 manifest_index = {};
        Mesh old_mesh = {};
        std::filesystem::path file_path = {};
    };

    struct MeshBuffers {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};
        Mesh mesh = {};
    };

    struct MeshUploadInfo {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};
        daxa::BufferId old_buffer = {};
        Mesh mesh = {};

        u32 manifest_index = {};
        u32 material_manifest_offset = {};
    };

    struct LoadTextureInfo {
        std::filesystem::path asset_path = {};
        BinaryAssetInfo* asset = {};
        u32 texture_index = {};
        u32 texture_manifest_index = {};
        u32 requested_resolution = {};
        daxa::ImageId old_image = {};
        std::filesystem::path image_path = {};
    };

    struct TextureOffsets {
        u32 width = {};
        u32 height = {};
        u32 size = {};
        u32 offset = {};
    };

    struct TextureUploadInfo {
        std::vector<TextureOffsets> offsets = {};
        daxa::BufferId staging_buffer = {};
        daxa::ImageId dst_image = {};
        daxa::ImageId old_image = {};
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

        void load_gltf_mesh(const LoadMeshInfo& info);
        void load_texture(const LoadTextureInfo& info);

        auto record_gpu_load_processing_commands() -> RecordCommands;

        static auto process_mesh(const ProcessMeshInfo& info) -> ProcessedMeshInfo;
        static void convert_gltf_to_binary(const std::filesystem::path& input_path, const std::filesystem::path& output_path);

        Context* context;

        std::vector<MeshUploadInfo> mesh_upload_queue = {};
        std::vector<TextureUploadInfo> texture_upload_queue = {};

        std::unique_ptr<std::mutex> mesh_upload_mutex = std::make_unique<std::mutex>();
        std::unique_ptr<std::mutex> texture_upload_mutex = std::make_unique<std::mutex>();
    };
}
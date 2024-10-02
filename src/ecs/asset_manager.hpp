#pragma once

#include "common/thread_pool.hpp"
#include "pch.hpp"
#include "graphics/context.hpp"
#include "asset_processor.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include "ecs/entity.hpp"
#include "ecs/scene.hpp"

namespace foundation {
    struct BinaryHeader {
        std::string name = {};
        u32 version = {};
    };

    struct BinaryMeshGroup {
        u32 mesh_offset = {};
        u32 mesh_count = {};

        static void serialize(ByteWriter& writer, const BinaryMeshGroup& value) {
            writer.write(value.mesh_offset);
            writer.write(value.mesh_count);
        }

        static auto deserialize(ByteReader& reader) -> BinaryMeshGroup { 
            BinaryMeshGroup value = {};
            reader.read(value.mesh_offset);
            reader.read(value.mesh_count);
            return value;    
        }
    };

    struct BinaryNode {
        std::optional<u32> mesh_index = {};
        glm::mat4 transform = {};
        std::vector<u32> children = {};
        std::string name = {};

        static void serialize(ByteWriter& writer, const BinaryNode& value) {
            writer.write(value.mesh_index);
            writer.write(value.transform);
            writer.write(value.children);
            writer.write(value.name);
        }

        static auto deserialize(ByteReader& reader) -> BinaryNode { 
            BinaryNode value = {};
            reader.read(value.mesh_index);
            reader.read(value.transform);
            reader.read(value.children);
            reader.read(value.name);
            return value;    
        }
    };

    struct BinaryTexture {
        struct BinaryMaterialIndex {
            MaterialType material_type = MaterialType::None;
            u32 material_index = {};

            static void serialize(ByteWriter& writer, const BinaryMaterialIndex& value) {
                writer.write(value.material_type);
                writer.write(value.material_index);
            }

            static auto deserialize(ByteReader& reader) -> BinaryMaterialIndex { 
                BinaryMaterialIndex value = {};
                reader.read(value.material_type);
                reader.read(value.material_index);
                return value;    
            }
        };
        std::vector<BinaryMaterialIndex> material_indices = {};
        std::string name = {};
        std::string file_path = {};

        static void serialize(ByteWriter& writer, const BinaryTexture& value) {
            writer.write(value.material_indices);
            writer.write(value.name);
            writer.write(value.file_path);
        }

        static auto deserialize(ByteReader& reader) -> BinaryTexture { 
            BinaryTexture value = {};
            reader.read(value.material_indices);
            reader.read(value.name);
            reader.read(value.file_path);
            return value;    
        }
    };

    struct BinaryMaterial {
        struct BinaryTextureInfo {
            u32 texture_index;
            u32 sampler_index;

            static void serialize(ByteWriter& writer, const BinaryTextureInfo& value) {
                writer.write(value.texture_index);
                writer.write(value.sampler_index);
            }

            static auto deserialize(ByteReader& reader) -> BinaryTextureInfo { 
                BinaryTextureInfo value = {};
                reader.read(value.texture_index);
                reader.read(value.sampler_index);
                return value;    
            }
        };
        std::optional<BinaryTextureInfo> albedo_info = {};
        std::optional<BinaryTextureInfo> alpha_mask_info = {};
        std::optional<BinaryTextureInfo> normal_info = {};
        std::optional<BinaryTextureInfo> roughness_metalness_info = {};
        std::optional<BinaryTextureInfo> roughness_info = {};
        std::optional<BinaryTextureInfo> metalness_info = {};
        std::optional<BinaryTextureInfo> emissive_info = {};
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor;
        u32 alpha_mode;
        f32 alpha_cutoff;
        bool double_sided;
        std::string name = {};

        static void serialize(ByteWriter& writer, const BinaryMaterial& value) {
            writer.write(value.albedo_info);
            writer.write(value.alpha_mask_info);
            writer.write(value.normal_info);
            writer.write(value.roughness_info);
            writer.write(value.metalness_info);
            writer.write(value.emissive_info);
            writer.write(value.metallic_factor);
            writer.write(value.roughness_factor);
            writer.write(value.emissive_factor);
            writer.write(value.alpha_mode);
            writer.write(value.alpha_cutoff);
            writer.write(value.double_sided);
            writer.write(value.name);
        }

        static auto deserialize(ByteReader& reader) -> BinaryMaterial { 
            BinaryMaterial value = {};
            reader.read(value.albedo_info);
            reader.read(value.alpha_mask_info);
            reader.read(value.normal_info);
            reader.read(value.roughness_info);
            reader.read(value.metalness_info);
            reader.read(value.emissive_info);
            reader.read(value.metallic_factor);
            reader.read(value.roughness_factor);
            reader.read(value.emissive_factor);
            reader.read(value.alpha_mode);
            reader.read(value.alpha_cutoff);
            reader.read(value.double_sided);
            reader.read(value.name);
            return value;    
        }
    };

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
        std::vector<BinaryNode> binary_nodes = {};
        std::vector<BinaryMeshGroup> binary_mesh_groups = {};
        std::vector<ProcessedMeshInfo> binary_meshes = {};
    };

    struct MeshManifestEntry {
        struct VirtualGeometryRenderInfo {
            daxa::BufferId mesh_buffer = {};
            u32 material_manifest_index = {};
        };

        u32 gltf_asset_manifest_index = {};
        u32 asset_local_mesh_index = {};
        u32 asset_local_primitive_index = {};
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
            MaterialType material_type = MaterialType::None;
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
        std::optional<TextureInfo> alpha_mask_info = {};
        std::optional<TextureInfo> normal_info = {};
        std::optional<TextureInfo> roughness_info = {};
        std::optional<TextureInfo> metalness_info = {};
        std::optional<TextureInfo> emissive_info = {};
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor;
        u32 alpha_mode;
        f32 alpha_cutoff;
        bool double_sided;
        u32 gltf_asset_manifest_index = {};
        u32 asset_local_index = {};
        u32 material_manifest_index = {};
        std::string name = {};
    };

    struct AssetManager {
        AssetManager(Context* _context, Scene* _scene);
        ~AssetManager();

        void load_model(LoadManifestInfo& info);

        static void convert_gltf_to_binary(LoadManifestInfo& info, const std::filesystem::path& output_path);

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
        std::vector<u32> dirty_meshes = {};
        std::vector<u32> dirty_mesh_groups = {};

        Context* context;
        Scene* scene;
        daxa::TaskBuffer gpu_meshes = {};
        daxa::TaskBuffer gpu_materials = {};
        daxa::TaskBuffer gpu_mesh_groups = {};
        daxa::TaskBuffer gpu_mesh_indices = {};

        daxa::TaskBuffer gpu_meshlet_data = {};
        daxa::TaskBuffer gpu_culled_meshlet_data = {};
        daxa::TaskBuffer gpu_meshlet_index_buffer = {};

        u32 total_meshlet_count = {};
        u32 total_triangle_count = {};
        u32 total_vertex_count = {};
    };
}
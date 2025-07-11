#pragma once

#include <utils/byte_utils.hpp>

namespace foundation {
    enum struct MaterialType : u32 {
        GltfAlbedo,
        GltfNormal,
        GltfRoughnessMetallic, // roughness stored in blue channel and metallic stored in green channel
        GltfEmissive,

        CompressedAlbedo, // BC1 only 3 channels
        CompressedAlphaMask, // BC4 optional alpha mask
        CompressedNormal, // BC5 compressed to 2 channels using orthographic compression
        CompressedMetalness, // BC4
        CompressedRoughness, // BC4
        CompressedEmissive, // BC1
        None
    };

    struct BinaryTextureFileFormat {
        u32 width = {};
        u32 height = {};
        u32 depth = {};
        daxa::Format format = {};
        std::vector<std::vector<std::byte>> mipmaps = {};

        static void serialize(ByteWriter& writer, const BinaryTextureFileFormat& value) {
            writer.write(value.width);
            writer.write(value.height);
            writer.write(value.depth);
            writer.write(value.format);
            writer.write(value.mipmaps);
        }

        static auto deserialize(ByteReader& reader) -> BinaryTextureFileFormat { 
            BinaryTextureFileFormat value = {};
            reader.read(value.width);
            reader.read(value.height);
            reader.read(value.depth);
            reader.read(value.format);
            reader.read(value.mipmaps);
            return value;    
        }
    };

    struct BinaryModelHeader {
        std::string name = {};
        u32 version = {};
        u32 meshlet_count = {}; // this exists to avoid stutters
        u32 triangle_count = {};
        u32 vertex_count = {};
        AABB model_aabb = {};

        static void serialize(ByteWriter& writer, const BinaryModelHeader& value) {
            writer.write(value.name);
            writer.write(value.version);
            writer.write(value.meshlet_count);
            writer.write(value.triangle_count);
            writer.write(value.vertex_count);
            writer.write(value.model_aabb);
        }

        static auto deserialize(ByteReader& reader) -> BinaryModelHeader { 
            BinaryModelHeader value = {};
            reader.read(value.name);
            reader.read(value.version);
            reader.read(value.meshlet_count);
            reader.read(value.triangle_count);
            reader.read(value.vertex_count);
            reader.read(value.model_aabb);
            return value;    
        }
    };

    struct BinaryMesh {
        std::optional<u32> material_index = {};
        u32 meshlet_count = {};
        std::string file_path = {};

        static void serialize(ByteWriter& writer, const BinaryMesh& value) {
            writer.write(value.material_index);
            writer.write(value.meshlet_count);
            writer.write(value.file_path);
        }

        static auto deserialize(ByteReader& reader) -> BinaryMesh { 
            BinaryMesh value = {};
            reader.read(value.material_index);
            reader.read(value.meshlet_count);
            reader.read(value.file_path);
            return value;    
        }
    };

    struct BinaryMeshGroup {
        u32 mesh_offset = {};
        u32 mesh_count = {};
        bool has_morph_targets = {};
        std::vector<f32> weights = {};
        std::string name = {};

        static void serialize(ByteWriter& writer, const BinaryMeshGroup& value) {
            writer.write(value.mesh_offset);
            writer.write(value.mesh_count);
            writer.write(value.has_morph_targets);
            writer.write(value.weights);
            writer.write(value.name);
        }

        static auto deserialize(ByteReader& reader) -> BinaryMeshGroup { 
            BinaryMeshGroup value = {};
            reader.read(value.mesh_offset);
            reader.read(value.mesh_count);
            reader.read(value.has_morph_targets);
            reader.read(value.weights);
            reader.read(value.name);
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
        u32 resolution = {};
        std::vector<BinaryMaterialIndex> material_indices = {};
        std::string name = {};
        std::string file_path = {};

        static void serialize(ByteWriter& writer, const BinaryTexture& value) {
            writer.write(value.resolution);
            writer.write(value.material_indices);
            writer.write(value.name);
            writer.write(value.file_path);
        }

        static auto deserialize(ByteReader& reader) -> BinaryTexture { 
            BinaryTexture value = {};
            reader.read(value.resolution);
            reader.read(value.material_indices);
            reader.read(value.name);
            reader.read(value.file_path);
            return value;    
        }
    };

    enum struct BinaryAlphaMode : u32 {
        Opaque,
        Masked,
        Transparent,
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
        f32vec4 albedo_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor;
        BinaryAlphaMode alpha_mode;
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
            writer.write(value.albedo_factor);
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
            reader.read(value.albedo_factor);
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

    struct BinaryAnimation {
        enum struct PathType : u32 {
            Position,
            Rotation,
            Scale,
            Weights,
        };

        enum struct InterpolationType : u32 {
            Linear,
            Step,
            CubicSpline
        };

        struct Channel {
            u32 sampler = {};
            u32 node = {};
            PathType path = {};

            static void serialize(ByteWriter& writer, const Channel& value) {
                writer.write(value.sampler);
                writer.write(value.node);
                writer.write(value.path);
            }

            static auto deserialize(ByteReader& reader) -> Channel { 
                Channel value = {};
                reader.read(value.sampler);
                reader.read(value.node);
                reader.read(value.path);
                return value;    
            }
        };

        struct Sampler {
            std::vector<f32> timestamps = {};
            std::vector<f32> values = {};
            InterpolationType interpolation = {};

            static void serialize(ByteWriter& writer, const Sampler& value) {
                writer.write(value.timestamps);
                writer.write(value.values);
                writer.write(value.interpolation);
            }

            static auto deserialize(ByteReader& reader) -> Sampler { 
                Sampler value = {};
                reader.read(value.timestamps);
                reader.read(value.values);
                reader.read(value.interpolation);
                return value;    
            }
        };

        std::vector<Channel> channels = {};
        std::vector<Sampler> samplers = {};
        std::string name = {};

        static void serialize(ByteWriter& writer, const BinaryAnimation& value) {
            writer.write(value.channels);
            writer.write(value.samplers);
            writer.write(value.name);
        }

        static auto deserialize(ByteReader& reader) -> BinaryAnimation { 
            BinaryAnimation value = {};
            reader.read(value.channels);
            reader.read(value.samplers);
            reader.read(value.name);
            return value;    
        }
    };

    struct BinaryAssetInfo {
        std::vector<BinaryNode> nodes = {};
        std::vector<BinaryMeshGroup> mesh_groups = {};
        std::vector<BinaryMesh> meshes = {};
        std::vector<BinaryMaterial> materials = {};
        std::vector<BinaryTexture> textures = {};
        std::vector<BinaryAnimation> animations = {};

        static void serialize(ByteWriter& writer, const BinaryAssetInfo& value) {
            writer.write(value.nodes);
            writer.write(value.mesh_groups);
            writer.write(value.meshes);
            writer.write(value.materials);
            writer.write(value.textures);
            writer.write(value.animations);
        }

        static auto deserialize(ByteReader& reader) -> BinaryAssetInfo { 
            BinaryAssetInfo value = {};
            reader.read(value.nodes);
            reader.read(value.mesh_groups);
            reader.read(value.meshes);
            reader.read(value.materials);
            reader.read(value.textures);
            reader.read(value.animations);
            return value;    
        }
    };
}
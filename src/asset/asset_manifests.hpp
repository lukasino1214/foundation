#pragma once

namespace foundation {
    struct Asset;
    struct Scene;
    struct Entity;

    struct AssetManifest {
        u32 texture_manifest_offset = {};
        u32 material_manifest_offset = {};
        u32 mesh_manifest_offset = {};
        u32 mesh_group_manifest_offset = {};
        u32 animation_manifest_offset = {};
        u32 skeleton_manifest_offset = {};

        u32 texture_manifest_size = {};
        u32 material_manifest_size = {};
        u32 mesh_manifest_size = {};
        u32 mesh_group_manifest_size = {};
        u32 animation_manifest_size = {};
        u32 skeleton_manifest_size = {};

        std::unique_ptr<Asset> asset = {};
    };

    struct LoadedRawImageData {
        std::vector<std::byte> loaded_data = {};
        std::span<std::byte const> data_span = {};
        std::string name = {};
    };

    struct LoadedImageInfo {
        std::filesystem::path file_path = {};
        std::string name = {};
    };

    struct LoadedMesh {
        std::vector<glm::vec3> positions = {};
        std::vector<glm::vec3> normals = {};
        std::vector<glm::vec2> uvs = {};
        std::vector<u32> indices = {};

        std::vector<Meshlet> meshlets = {};
        std::vector<u32> indirect_vertices = {};
        std::vector<u8> micro_indices = {};
        std::vector<BoundingSphere> bounding_spheres = {};
        std::vector<AABB> aabbs = {};

        AABB mesh_aabb;

        std::string name = {};
    };

    struct Asset {
        virtual ~Asset() = default;

        virtual void load_entites(const AssetManifest& asset_manifest, Scene* scene, Entity& root_entity) = 0;
        virtual auto load_image(u32 image_index) -> std::variant<LoadedRawImageData, LoadedImageInfo> = 0;
        virtual auto load_mesh(u32 mesh_group_index, u32 mesh_index) -> LoadedMesh = 0;
    };

    enum struct MaterialType : u32 {
        Opaque,
        Masked,
        Blend,
    };

    enum struct TextureType : u32 {
        GltfAlbedo,
        GltfNormal,
        GltfRoughnessMetallic, // roughness stored in blue channel and metallic stored in green channel
        GltfEmissive,
        None,
    };

    struct TextureManifest {
        struct MaterialManifestIndex {
            TextureType texture_type = TextureType::None;
            u32 asset_local_material_index = {};
        };

        struct RenderInfo {
            daxa::ImageId image_id = {};
            daxa::SamplerId sampler_id = {};
        };

        u32 asset_manifest_index = {};
        u32 asset_local_texture_index = {};

        std::string name = {};
        std::vector<MaterialManifestIndex> material_manifest_indices = {};
        std::optional<RenderInfo> render_info = {};
    };

    struct MaterialManifest {
        struct TextureInfo {
            u32 asset_local_texture_index;
            u32 sampler_index;
        };

        u32 asset_manifest_index = {};
        u32 asset_material_local_index = {};

        std::optional<TextureInfo> albedo_info = {};
        std::optional<TextureInfo> normal_info = {};
        std::optional<TextureInfo> roughness_metalness_info = {};
        std::optional<TextureInfo> emissive_info = {};
        glm::vec4 albedo_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
        f32 metallic_factor;
        f32 roughness_factor;
        glm::vec3 emissive_factor = { 0.0f, 0.0f, 0.0f };
        MaterialType material_type;
        f32 alpha_cutoff;
        bool double_sided;
        std::string name = {};
    };

    struct MeshManifest {
        struct RenderInfo {
            daxa::BufferId mesh_buffer = {};
            daxa::BufferId index_buffer = {};
            daxa::BlasId blas = {};
            MeshGeometryData mesh = {};
            u32 vertex_count = {};
            u32 index_count = {};
            u32 meshlet_count = {};
        };

        u32 asset_manifest_index = {};
        u32 asset_local_mesh_group_index = {};
        u32 asset_local_mesh_index = {};

        std::optional<u32> asset_local_material_index = {};
        std::optional<RenderInfo> render_info = {};
    };

    struct MeshGroupManifest {
        u32 asset_manifest_index = {};
        u32 asset_local_mesh_group_index = {};
        u32 asset_local_mesh_manifest_indices_offset = {};

        u32 mesh_count = {};
        std::string name = {};
    };

    enum struct AnimationPath : u8 {
        Translation,
        Rotation,
        Scale,
        Weights,
    };

    enum struct AnimationInterpolation : u8 {
        Linear,
        Step,
        CubicSpline,
    };

    struct AnimationChannel {
        std::optional<u32> node_index = {};
        u32 sampler_index = {};
        AnimationPath path = AnimationPath::Translation;
    };

    struct AnimationSampler {
        std::vector<f32> timestamps = {};
        std::vector<f32> values = {};
        AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    };

    struct AnimationManifest {
        u32 asset_manifest_index = {};
        u32 asset_local_animation_index = {};

        std::vector<AnimationChannel> channels = {};
        std::vector<AnimationSampler> samplers = {};

        f32 min_timestamp = {};
        f32 max_timestamp = {};

        std::string name = {};
    };

    struct SkeletonManifest {
        u32 asset_manifest_index = {};
        u32 asset_local_skeleton_index = {};

        std::vector<glm::mat4> inverse_bind_matrices = {};
        std::optional<u32> root_node_index = {};
        std::vector<u32> joint_node_indices = {};

        std::string name = {};
    };

    struct LoadedAsset {
        std::vector<TextureManifest> texture_manifests = {};
        std::vector<MaterialManifest> material_manifests = {};
        std::vector<MeshManifest> mesh_manifests = {};
        std::vector<MeshGroupManifest> mesh_group_manifests = {};
        std::vector<AnimationManifest> animation_manifests = {};
        std::vector<SkeletonManifest> skeleton_manifests = {};
        std::unique_ptr<Asset> asset = {};
    };
}
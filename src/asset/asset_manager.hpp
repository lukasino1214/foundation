#pragma once

#include "asset_processor.hpp"
#include <ecs/entity.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <fastgltf/types.hpp>
#pragma clang diagnostic pop

namespace foundation {
    struct GPUContext;
    struct ThreadPool;

    enum struct MaterialType : u32 {
        GltfAlbedo,
        GltfNormal,
        GltfRoughnessMetallic, // roughness stored in blue channel and metallic stored in green channel
        GltfEmissive,
        None,
    };

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

        std::unique_ptr<fastgltf::Asset> asset = {};
    };

    struct TextureManifest {
        struct MaterialManifestIndex {
            MaterialType material_type = MaterialType::None;
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
        u32 alpha_mode;
        f32 alpha_cutoff;
        bool double_sided;
        std::string name = {};
    };

    struct MeshManifest {
        struct RenderInfo {
            daxa::BufferId mesh_buffer = {};
            daxa::BufferId index_buffer = {};
            MeshGeometryData mesh = {};
            u32 vertex_count = {};
            u32 index_count = {};
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

    struct AssetManager {
        explicit AssetManager(GPUContext* _gpu_context, AssetProcessor* _asset_processor, ThreadPool* _thread_pool, Scene* _scene);
        ~AssetManager();

        void load_asset(const std::filesystem::path& file_path, Entity root_entity);

        void unload_asset(const std::filesystem::path& file_path);
        void unload_asset(u32 asset_index);

        void update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos);
    
    private:
        void load_asset_to_manifests(fastgltf::Asset* asset, const std::filesystem::path& file_path, AssetManifest* asset_manifest, u32 asset_manifest_index);

    public:

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
        daxa::TaskBuffer gpu_meshes = {};
    };
}
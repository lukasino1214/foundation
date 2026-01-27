#pragma once

namespace fastgltf {
    class Asset;
}

namespace foundation {
    struct GPUContext; 

    struct LoadedImage {
        daxa::Format format = daxa::Format::UNDEFINED;
        std::vector<std::byte> data = {};
        std::vector<std::span<std::byte const>> mipmaps = {};
        u32 width = 1;
        u32 height = 1;
        u32 depth = 1;
    };

    struct LoadedMesh {
        std::vector<glm::vec3> positions = {};
        std::vector<glm::vec3> normals = {};
        std::vector<glm::vec2> uvs = {};
        std::vector<u32> indices = {};
    };

    struct LoadMeshInfo {
        fastgltf::Asset* asset;
        std::filesystem::path asset_path = {};

        u32 asset_manifest_index = {};
        u32 mesh_manifest_index = {};

        u32 asset_local_mesh_group_index = {};
        u32 asset_local_mesh_index = {};
    };

    struct MeshUploadInfo {
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};

        daxa::BufferId staging_index_buffer = {};
        daxa::BufferId index_buffer = {};

        MeshGeometryData mesh_geometry_info = {};

        u32 asset_manifest_index = {};
        u32 mesh_manifest_index = {};
    };

    struct LoadTextureInfo {
        fastgltf::Asset* asset;
        std::filesystem::path asset_path = {};
        bool load_as_srgb = {};
        
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

        static auto load_mesh(fastgltf::Asset* asset, const std::filesystem::path& file_path, u32 mesh_group_index, u32 mesh_index) -> LoadedMesh;
        static auto load_image(const std::filesystem::path& file_path) -> LoadedImage;

        void process_mesh_load(const LoadMeshInfo& info);
        void process_texture_load(const LoadTextureInfo& info);

        auto upload_resources(daxa::CommandRecorder& cmd_recorder) -> ResourceUploadInfos;

    private:
        static auto load_dds(const std::filesystem::path& file_path) -> LoadedImage;
        static auto load_png(const std::filesystem::path& file_path) -> LoadedImage;
        static auto load_jpeg(const std::filesystem::path& file_path) -> LoadedImage;

        GPUContext* gpu_context = {};

        std::unique_ptr<std::mutex> mesh_upload_mutex = std::make_unique<std::mutex>();
        std::unique_ptr<std::mutex> texture_upload_mutex = std::make_unique<std::mutex>();

        std::vector<MeshUploadInfo> mesh_upload_queue = {};
        std::vector<TextureUploadInfo> texture_upload_queue = {};
    };
}
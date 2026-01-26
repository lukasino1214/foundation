#pragma once

namespace fastgltf {
    class Asset;
}

namespace foundation {
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

    struct AssetProcessor {
        static auto load_mesh(fastgltf::Asset* asset, const std::filesystem::path& file_path, u32 mesh_group_index, u32 mesh_index) -> LoadedMesh;
        static auto load_image(const std::filesystem::path& file_path) -> LoadedImage;

    private:
        static auto load_dds(const std::filesystem::path& file_path) -> LoadedImage;
        static auto load_png(const std::filesystem::path& file_path) -> LoadedImage;
        static auto load_jpeg(const std::filesystem::path& file_path) -> LoadedImage;
    };
}
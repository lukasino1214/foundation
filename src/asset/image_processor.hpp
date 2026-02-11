#pragma once

namespace foundation {
    struct LoadedImage {
        daxa::Format format = daxa::Format::UNDEFINED;
        std::vector<std::byte> data = {};
        std::vector<std::span<std::byte const>> mipmaps = {};
        u32 width = 1;
        u32 height = 1;
        u32 depth = 1;
    };
    
    struct ImageProcessor {
        static auto load_image(const std::filesystem::path& file_path) -> LoadedImage;
        static auto load_image(std::span<std::byte const> data) -> LoadedImage;

    private:
        static auto load_dds(std::span<std::byte const> data) -> LoadedImage;
        static auto load_png(std::span<std::byte const> data) -> LoadedImage;
        static auto load_jpeg(std::span<std::byte const> data) -> LoadedImage;
    };
}
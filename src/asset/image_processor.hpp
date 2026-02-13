#pragma once

#include "asset_manifests.hpp"

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

        static auto compress_image(LoadedImage& image, TextureType texture_type) -> LoadedImage;

    private:
        static auto load_dds(std::span<std::byte const> data) -> LoadedImage;
        static auto load_png(std::span<std::byte const> data) -> LoadedImage;
        static auto load_jpeg(std::span<std::byte const> data) -> LoadedImage;

        static void initialize_compressor();
        
        static auto compress_to_bc1(LoadedImage& image, bool is_srgb) -> LoadedImage;
        static auto compress_to_bc3(LoadedImage& image, bool is_srgb) -> LoadedImage;
        static auto compress_to_bc5_rg(LoadedImage& image) -> LoadedImage;
        static auto compress_to_bc5_normal(LoadedImage& image) -> LoadedImage;
        
        static auto generate_mipmaps(LoadedImage& image, bool preserve_alpha_coverage = false, f32 alpha_ref = 0.5f) -> LoadedImage;
        static void downsample_rgba8(const std::byte* src, std::byte* dst, u32 src_width, u32 src_height);
        static void downsample_rgba8_premultiplied(const std::byte* src, std::byte* dst, u32 src_width, u32 src_height, bool is_srgb);
        
        static auto has_alpha_channel(const LoadedImage& image) -> bool;
        static auto compute_alpha_coverage(const std::byte* data, u32 pixel_count, f32 alpha_ref) -> f32;
        static void scale_alpha_to_coverage(std::byte* data, u32 pixel_count, f32 desired_coverage, f32 alpha_ref);
    };
} // namespace foundation
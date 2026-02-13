#include "image_processor.hpp"

#include <utils/io.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"

#include <dds.hpp>

#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__JPEG
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ZLIB

#define WUFFS_CONFIG__STATIC_FUNCTIONS

#ifndef WUFFS_IMPLEMENTATION
#define WUFFS_IMPLEMENTATION
#endif

#include <wuffs-v0.4.c>

#include <bc7enc.h>
#include <rgbcx.h>

#pragma clang diagnostic pop

namespace foundation {
    enum class ImageType {
        Unknown,
        PNG,
        JPEG,
        DDS
    };

    static auto detect_image_type(std::span<std::byte const> data) -> ImageType {
        if(data.size() < 8) {
            return ImageType::Unknown;
        }

        // PNG magic bytes: 89 50 4E 47 0D 0A 1A 0A
        constexpr std::array<std::byte, 8> png_magic = {
            std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
            std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A},
        };
        if(std::memcmp(data.data(), png_magic.data(), 8) == 0) {
            return ImageType::PNG;
        }

        // JPEG magic bytes: FF D8 FF (SOI marker + APP0/APP1/etc)
        if(data.size() >= 3 &&
           data[0] == std::byte{0xFF} &&
           data[1] == std::byte{0xD8} &&
           data[2] == std::byte{0xFF}) {
            return ImageType::JPEG;
        }

        // DDS magic bytes: "DDS " (44 44 53 20)
        if(data.size() >= 4 &&
           data[0] == std::byte{0x44} &&
           data[1] == std::byte{0x44} &&
           data[2] == std::byte{0x53} &&
           data[3] == std::byte{0x20}) {
            return ImageType::DDS;
        }

        return ImageType::Unknown;
    }

    static auto dds_format_to_daxa_format(DXGI_FORMAT format, bool alpha_flag) -> daxa::Format {
        switch(format) {
            case DXGI_FORMAT_BC1_UNORM: {
                if(alpha_flag) {
                    return daxa::Format::BC1_RGBA_UNORM_BLOCK;
                } else {
                    return daxa::Format::BC1_RGB_UNORM_BLOCK;
                }
            }
            case DXGI_FORMAT_BC1_UNORM_SRGB: {
                if(alpha_flag) {
                    return daxa::Format::BC1_RGBA_SRGB_BLOCK;
                } else {
                    return daxa::Format::BC1_RGB_SRGB_BLOCK;
                }
            }

            case DXGI_FORMAT_BC2_UNORM:
                return daxa::Format::BC2_UNORM_BLOCK;
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                return daxa::Format::BC2_SRGB_BLOCK;
            case DXGI_FORMAT_BC3_UNORM:
                return daxa::Format::BC3_UNORM_BLOCK;
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                return daxa::Format::BC3_SRGB_BLOCK;
            case DXGI_FORMAT_BC4_UNORM:
                return daxa::Format::BC4_UNORM_BLOCK;
            case DXGI_FORMAT_BC4_SNORM:
                return daxa::Format::BC4_SNORM_BLOCK;
            case DXGI_FORMAT_BC5_UNORM:
                return daxa::Format::BC5_UNORM_BLOCK;
            case DXGI_FORMAT_BC5_SNORM:
                return daxa::Format::BC5_SNORM_BLOCK;
            case DXGI_FORMAT_BC7_UNORM:
                return daxa::Format::BC7_UNORM_BLOCK;
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return daxa::Format::BC7_SRGB_BLOCK;

            // 8-bit wide formats
            case DXGI_FORMAT_R8_UNORM:
                return daxa::Format::R8_UNORM;
            case DXGI_FORMAT_R8_UINT:
                return daxa::Format::R8_UINT;
            case DXGI_FORMAT_R8_SNORM:
                return daxa::Format::R8_SNORM;
            case DXGI_FORMAT_R8_SINT:
                return daxa::Format::R8_SINT;
    #if defined(VK_KHR_maintenance5)
            case DXGI_FORMAT_A8_UNORM:
                return daxa::Format::A8_UNORM_KHR;
    #endif

            // 16-bit wide formats
            case DXGI_FORMAT_R8G8_UNORM:
                return daxa::Format::R8G8_UNORM;
            case DXGI_FORMAT_R8G8_UINT:
                return daxa::Format::R8G8_UINT;
            case DXGI_FORMAT_R8G8_SNORM:
                return daxa::Format::R8G8_SNORM;
            case DXGI_FORMAT_R8G8_SINT:
                return daxa::Format::R8G8_SINT;

            case DXGI_FORMAT_R16_FLOAT:
                return daxa::Format::R16_SFLOAT;
            case DXGI_FORMAT_R16_UNORM:
                return daxa::Format::R16_UNORM;
            case DXGI_FORMAT_R16_UINT:
                return daxa::Format::R16_UINT;
            case DXGI_FORMAT_R16_SNORM:
                return daxa::Format::R16_SNORM;
            case DXGI_FORMAT_R16_SINT:
                return daxa::Format::R16_SINT;

            case DXGI_FORMAT_B5G5R5A1_UNORM:
                return daxa::Format::B5G5R5A1_UNORM_PACK16;
            case DXGI_FORMAT_B5G6R5_UNORM:
                return daxa::Format::B5G6R5_UNORM_PACK16;
            case DXGI_FORMAT_B4G4R4A4_UNORM:
                return daxa::Format::B4G4R4A4_UNORM_PACK16;

            // 32-bit wide formats
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                return daxa::Format::R8G8B8A8_UNORM;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                return daxa::Format::R8G8B8A8_SRGB;
            case DXGI_FORMAT_R8G8B8A8_UINT:
                return daxa::Format::R8G8B8A8_UINT;
            case DXGI_FORMAT_R8G8B8A8_SNORM:
                return daxa::Format::R8G8B8A8_SNORM;
            case DXGI_FORMAT_R8G8B8A8_SINT:
                return daxa::Format::R8G8B8A8_SINT;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                return daxa::Format::B8G8R8A8_UNORM;
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                return daxa::Format::B8G8R8A8_SRGB;

            case DXGI_FORMAT_R16G16_FLOAT:
                return daxa::Format::R16G16_SFLOAT;
            case DXGI_FORMAT_R16G16_UNORM:
                return daxa::Format::R16G16_UNORM;
            case DXGI_FORMAT_R16G16_UINT:
                return daxa::Format::R16G16_UINT;
            case DXGI_FORMAT_R16G16_SNORM:
                return daxa::Format::R16G16_SNORM;
            case DXGI_FORMAT_R16G16_SINT:
                return daxa::Format::R16G16_SINT;

            case DXGI_FORMAT_R32_FLOAT:
                return daxa::Format::R32_SFLOAT;
            case DXGI_FORMAT_R32_UINT:
                return daxa::Format::R32_UINT;
            case DXGI_FORMAT_R32_SINT:
                return daxa::Format::R32_SINT;

            case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
                return daxa::Format::E5B9G9R9_UFLOAT_PACK32;
            case DXGI_FORMAT_R10G10B10A2_UNORM:
                return daxa::Format::A2B10G10R10_UNORM_PACK32;
            case DXGI_FORMAT_R10G10B10A2_UINT:
                return daxa::Format::A2B10G10R10_UINT_PACK32;
            case DXGI_FORMAT_R11G11B10_FLOAT:
                return daxa::Format::B10G11R11_UFLOAT_PACK32;

            // 64-bit wide formats
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
                return daxa::Format::R16G16B16A16_SFLOAT;
            case DXGI_FORMAT_R16G16B16A16_SINT:
                return daxa::Format::R16G16B16A16_SINT;
            case DXGI_FORMAT_R16G16B16A16_UINT:
                return daxa::Format::R16G16B16A16_UINT;
            case DXGI_FORMAT_R16G16B16A16_UNORM:
                return daxa::Format::R16G16B16A16_UNORM;
            case DXGI_FORMAT_R16G16B16A16_SNORM:
                return daxa::Format::R16G16B16A16_SNORM;

            case DXGI_FORMAT_R32G32_FLOAT:
                return daxa::Format::R32G32_SFLOAT;
            case DXGI_FORMAT_R32G32_UINT:
                return daxa::Format::R32G32_UINT;
            case DXGI_FORMAT_R32G32_SINT:
                return daxa::Format::R32G32_SINT;

            // 96-bit wide formats
            case DXGI_FORMAT_R32G32B32_FLOAT:
                return daxa::Format::R32G32B32_SFLOAT;
            case DXGI_FORMAT_R32G32B32_UINT:
                return daxa::Format::R32G32B32_UINT;
            case DXGI_FORMAT_R32G32B32_SINT:
                return daxa::Format::R32G32B32_SINT;

            // 128-bit wide formats
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
                return daxa::Format::R32G32B32A32_SFLOAT;
            case DXGI_FORMAT_R32G32B32A32_UINT:
                return daxa::Format::R32G32B32A32_UINT;
            case DXGI_FORMAT_R32G32B32A32_SINT:
                return daxa::Format::R32G32B32A32_SINT;

            case DXGI_FORMAT_R8G8_B8G8_UNORM:
            case DXGI_FORMAT_G8R8_G8B8_UNORM:
            case DXGI_FORMAT_YUY2:
            default:
                return daxa::Format::UNDEFINED;
        }
    }

    auto ImageProcessor::load_image(const std::filesystem::path& file_path) -> LoadedImage {
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error(fmt::format("File does not exist with path: {}", file_path.string()));
        }

        std::filesystem::path extension = file_path.extension();
        std::vector<std::byte> file_data = io::read_file(file_path);

        if(extension == ".dds") {
            return load_dds(file_data);
        } else if(extension == ".png") {
            return load_png(file_data);
        } else if(extension == ".jpg" || extension == ".jpeg") {
            return load_jpeg(file_data);
        } else {
            throw std::runtime_error(fmt::format("Unsupported image format: {}", file_path.string()));
        }
    }

    auto ImageProcessor::load_image(std::span<std::byte const> data) -> LoadedImage {
        ImageType image_type = detect_image_type(data);

        switch(image_type) {
            case ImageType::PNG: {
                return load_png(data);
                break;
            }
            case ImageType::JPEG: {
                return load_jpeg(data);
                break;
            }
            case ImageType::DDS: {
                return load_dds(data);
                break;
            }
            case ImageType::Unknown:
            default: {
                throw std::runtime_error("Unknown or unsupported image format for embedded image");
            }
        }
    }

    auto ImageProcessor::load_dds(std::span<std::byte const> data) -> LoadedImage {
        PROFILE_SCOPE;
        dds::Image dds_image = {};
        dds::ReadResult result = dds::readImage(const_cast<u8*>(r_cast<const u8*>(data.data())), data.size_bytes(), &dds_image);
        if(result != dds::ReadResult::Success) {
            throw std::runtime_error("something went wrong");
        }

        LoadedImage loaded_image = {};
        loaded_image.format = dds_format_to_daxa_format(dds_image.format, dds_image.supportsAlpha);
        loaded_image.width = dds_image.width;
        loaded_image.height = dds_image.height;
        loaded_image.depth = dds_image.depth;

        usize total_data_size = 0;
        for(const auto& mipmap : dds_image.mipmaps) {
            total_data_size += mipmap.size();
        }
        loaded_image.data.resize(total_data_size);

        usize current_offset = 0;
        loaded_image.mipmaps.reserve(dds_image.mipmaps.size());
        for(const auto& mipmap : dds_image.mipmaps) {
            std::memcpy(loaded_image.data.data() + current_offset, mipmap.data(), mipmap.size());
            loaded_image.mipmaps.emplace_back(loaded_image.data.data() + current_offset, mipmap.size());
            current_offset += mipmap.size();
        }

        return loaded_image;
    }

#define WORKBUF_STACK_SIZE 8192

    auto ImageProcessor::load_png(std::span<std::byte const> data) -> LoadedImage {
        PROFILE_SCOPE;

        LoadedImage loaded_image = {};

        wuffs_png__decoder dec;
        wuffs_base__status status = wuffs_png__decoder__initialize(&dec, sizeof(wuffs_png__decoder), WUFFS_VERSION, 0);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        wuffs_png__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);

        wuffs_base__io_buffer io_buf = wuffs_base__ptr_u8__reader(const_cast<u8*>(r_cast<const u8*>(data.data())), data.size(), true);
        wuffs_base__image_config config = {};

        status = wuffs_png__decoder__decode_image_config(&dec, &config, &io_buf);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        uint32_t width = wuffs_base__pixel_config__width(&config.pixcfg);
        uint32_t height = wuffs_base__pixel_config__height(&config.pixcfg);

        loaded_image.width = width;
        loaded_image.height = height;
        loaded_image.format = daxa::Format::R8G8B8A8_UNORM;

        wuffs_base__pixel_format src_pfmt = wuffs_base__pixel_config__pixel_format(&config.pixcfg);
        uint32_t coloration = wuffs_base__pixel_format__coloration(&src_pfmt);
        uint32_t transparency = wuffs_base__pixel_format__transparency(&src_pfmt);

        int src_channels = 0;
        if(coloration == WUFFS_BASE__PIXEL_COLORATION__GRAY) {
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 1 : 2;
        } else { // RICH
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 3 : 4;
        }

        int desired_channels = 4;
        // if (channels_in_file) *channels_in_file = src_channels;

        int n = (desired_channels != 0) ? desired_channels : src_channels;
        uint32_t dst_pfmt = 0;
        bool is_16bit = false;

        if(is_16bit) {
            dst_pfmt = (n == 1) ? WUFFS_BASE__PIXEL_FORMAT__Y_16LE : WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE;
            if(n == 3)
                n = 4; // Force 4 channels for RGB -> RGBA conversion in 16-bit mode
        } else {
            switch(n) {
                case 1: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__Y; break;
                case 2: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL; break;
                case 3: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGB; break;
                default: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL; break;
            }
        }

        wuffs_base__pixel_config__set(&config.pixcfg, dst_pfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);
        usize bytes_per_pixel = s_cast<usize>(is_16bit ? (n + n) : n);

        if(width == 0 || height == 0 || bytes_per_pixel == 0 ||
           s_cast<usize>(width) > SIZE_MAX / height ||
           s_cast<usize>(width * height) > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        usize pixel_buf_size = s_cast<usize>(width * height) * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_png__decoder__workbuf_len(&dec);
        void* workbuf_ptr = nullptr;

        wuffs_base__slice_u8 workbuf = {};

        if(workbuf_len_range.max_incl > 0) {
            usize wb_len = s_cast<usize>(workbuf_len_range.max_incl);
            if(wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            u8 workbuf_stack[WORKBUF_STACK_SIZE];
            if(wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if(!workbuf_ptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = s_cast<u8*>(workbuf_ptr);
                workbuf.len = wb_len;
            }
        }

        wuffs_base__pixel_buffer pb = {};
        status = wuffs_base__pixel_buffer__set_from_slice(&pb, &config.pixcfg, wuffs_base__make_slice_u8(r_cast<u8*>(loaded_image.data.data()), pixel_buf_size));
        if(!wuffs_base__status__is_ok(&status)) {
            if(workbuf_ptr != nullptr) {
                free(workbuf_ptr);
            }
            throw std::runtime_error("something went wrong");
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
        status = wuffs_png__decoder__decode_frame(&dec, &pb, &io_buf, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);
#pragma clang diagnostic pop

        if(workbuf_ptr != nullptr) {
            free(workbuf_ptr);
        }

        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        return loaded_image;
    }

    auto ImageProcessor::load_jpeg(std::span<std::byte const> data) -> LoadedImage {
        PROFILE_SCOPE;

        LoadedImage loaded_image = {};

        wuffs_jpeg__decoder dec;
        wuffs_base__status status = wuffs_jpeg__decoder__initialize(&dec, sizeof(wuffs_jpeg__decoder), WUFFS_VERSION, 0);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        wuffs_jpeg__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);

        wuffs_base__io_buffer io_buf = wuffs_base__ptr_u8__reader(const_cast<u8*>(r_cast<const u8*>(data.data())), data.size(), true);
        wuffs_base__image_config config = {};

        status = wuffs_jpeg__decoder__decode_image_config(&dec, &config, &io_buf);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        uint32_t width = wuffs_base__pixel_config__width(&config.pixcfg);
        uint32_t height = wuffs_base__pixel_config__height(&config.pixcfg);

        loaded_image.width = width;
        loaded_image.height = height;
        loaded_image.format = daxa::Format::R8G8B8A8_UNORM;

        wuffs_base__pixel_format src_pfmt = wuffs_base__pixel_config__pixel_format(&config.pixcfg);
        uint32_t coloration = wuffs_base__pixel_format__coloration(&src_pfmt);
        uint32_t transparency = wuffs_base__pixel_format__transparency(&src_pfmt);

        int src_channels = 0;
        if(coloration == WUFFS_BASE__PIXEL_COLORATION__GRAY) {
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 1 : 2;
        } else { // RICH
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 3 : 4;
        }

        int desired_channels = 4;
        // if (channels_in_file) *channels_in_file = src_channels;

        int n = (desired_channels != 0) ? desired_channels : src_channels;
        uint32_t dst_pfmt = 0;

        switch(n) {
            case 1: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__Y; break;
            case 2: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL; break;
            case 3: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGB; break;
            default: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL; break;
        }

        wuffs_base__pixel_config__set(&config.pixcfg, dst_pfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);

        usize bytes_per_pixel = s_cast<usize>(n);

        if(width == 0 || height == 0 || bytes_per_pixel == 0 ||
           s_cast<usize>(width) > SIZE_MAX / height ||
           s_cast<usize>(width * height) > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        usize pixel_buf_size = s_cast<usize>(width * height) * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_jpeg__decoder__workbuf_len(&dec);
        void* workbuf_ptr = nullptr;

        wuffs_base__slice_u8 workbuf = {};

        if(workbuf_len_range.max_incl > 0) {
            usize wb_len = s_cast<usize>(workbuf_len_range.max_incl);
            if(wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            u8 workbuf_stack[WORKBUF_STACK_SIZE];
            if(wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if(workbuf_ptr == nullptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = s_cast<u8*>(workbuf_ptr);
                workbuf.len = wb_len;
            }
        }

        wuffs_base__pixel_buffer pb = {};
        status = wuffs_base__pixel_buffer__set_from_slice(&pb, &config.pixcfg, wuffs_base__make_slice_u8(r_cast<u8*>(loaded_image.data.data()), pixel_buf_size));
        if(!wuffs_base__status__is_ok(&status)) {
            if(workbuf_ptr != nullptr)
                free(workbuf_ptr);
            throw std::runtime_error("something went wrong");
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
        status = wuffs_jpeg__decoder__decode_frame(&dec, &pb, &io_buf, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);
#pragma clang diagnostic pop

        if(workbuf_ptr != nullptr) {
            free(workbuf_ptr);
        }

        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        return loaded_image;
    }

    void ImageProcessor::initialize_compressor() {
        static bool compressor_initialized = false;
        if(!compressor_initialized) {
            bc7enc_compress_block_init();
            rgbcx::init();
            compressor_initialized = true;
        }
    }

    auto ImageProcessor::has_alpha_channel(const LoadedImage& image) -> bool {
        PROFILE_SCOPE;

        for(usize i = 3; i < image.data.size(); i += 4) {
            if(s_cast<u8>(image.data[i]) < 255) {
                return true;
            }
        }

        return false;
    }

    void ImageProcessor::downsample_rgba8(const std::byte *src, std::byte *dst, u32 src_width, u32 src_height) {
        PROFILE_SCOPE;

        u32 dst_width = std::max(1u, src_width / 2);
        u32 dst_height = std::max(1u, src_height / 2);

        for(u32 y = 0; y < dst_height; y++) {
            for(u32 x = 0; x < dst_width; x++) {
                u32 src_x = x * 2;
                u32 src_y = y * 2;

                u32 r = 0, g = 0, b = 0, a = 0;
                for(u32 dy = 0; dy < 2 && (src_y + dy) < src_height; dy++) {
                    for(u32 dx = 0; dx < 2 && (src_x + dx) < src_width; dx++) {
                        u32 src_idx = ((src_y + dy) * src_width + (src_x + dx)) * 4;
                        r += s_cast<u32>(src[src_idx + 0]);
                        g += s_cast<u32>(src[src_idx + 1]);
                        b += s_cast<u32>(src[src_idx + 2]);
                        a += s_cast<u32>(src[src_idx + 3]);
                    }
                }

                u32 dst_idx = (y * dst_width + x) * 4;
                dst[dst_idx + 0] = s_cast<std::byte>(r / 4);
                dst[dst_idx + 1] = s_cast<std::byte>(g / 4);
                dst[dst_idx + 2] = s_cast<std::byte>(b / 4);
                dst[dst_idx + 3] = s_cast<std::byte>(a / 4);
            }
        }
    }

    void ImageProcessor::downsample_rgba8_premultiplied(const std::byte* src, std::byte* dst, u32 src_width, u32 src_height, bool is_srgb) {
        PROFILE_SCOPE;

        u32 dst_width = std::max(1u, src_width / 2);
        u32 dst_height = std::max(1u, src_height / 2);

        auto srgb_to_linear = [](u8 v) -> f32 {
            f32 f = s_cast<f32>(v) / 255.0f;
            return (f <= 0.04045f) ? f / 12.92f : std::pow((f + 0.055f) / 1.055f, 2.4f);
        };

        auto linear_to_srgb = [](f32 v) -> u8 {
            f32 s = (v <= 0.0031308f) ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            return s_cast<u8>(std::clamp(s * 255.0f + 0.5f, 0.0f, 255.0f));
        };

        for(u32 y = 0; y < dst_height; y++) {
            for(u32 x = 0; x < dst_width; x++) {
                u32 src_x = x * 2;
                u32 src_y = y * 2;

                f32 pr = 0.0f, pg = 0.0f, pb = 0.0f, pa = 0.0f;
                u32 sample_count = 0;

                for(u32 dy = 0; dy < 2 && (src_y + dy) < src_height; dy++) {
                    for(u32 dx = 0; dx < 2 && (src_x + dx) < src_width; dx++) {
                        u32 src_idx = ((src_y + dy) * src_width + (src_x + dx)) * 4;
                        f32 a = s_cast<f32>(s_cast<u8>(src[src_idx + 3])) / 255.0f;

                        f32 r, g, b;
                        if(is_srgb) {
                            r = srgb_to_linear(s_cast<u8>(src[src_idx + 0]));
                            g = srgb_to_linear(s_cast<u8>(src[src_idx + 1]));
                            b = srgb_to_linear(s_cast<u8>(src[src_idx + 2]));
                        } else {
                            r = s_cast<f32>(s_cast<u8>(src[src_idx + 0])) / 255.0f;
                            g = s_cast<f32>(s_cast<u8>(src[src_idx + 1])) / 255.0f;
                            b = s_cast<f32>(s_cast<u8>(src[src_idx + 2])) / 255.0f;
                        }

                        pr += r * a;
                        pg += g * a;
                        pb += b * a;
                        pa += a;
                        sample_count++;
                    }
                }

                f32 inv_count = 1.0f / s_cast<f32>(sample_count);
                pr *= inv_count;
                pg *= inv_count;
                pb *= inv_count;
                pa *= inv_count;

                f32 r, g, b;
                if(pa > 1e-6f) {
                    r = pr / pa;
                    g = pg / pa;
                    b = pb / pa;
                } else {
                    r = g = b = 0.0f;
                }

                u32 dst_idx = (y * dst_width + x) * 4;
                if(is_srgb) {
                    dst[dst_idx + 0] = s_cast<std::byte>(linear_to_srgb(r));
                    dst[dst_idx + 1] = s_cast<std::byte>(linear_to_srgb(g));
                    dst[dst_idx + 2] = s_cast<std::byte>(linear_to_srgb(b));
                } else {
                    dst[dst_idx + 0] = s_cast<std::byte>(s_cast<u8>(std::clamp(r * 255.0f + 0.5f, 0.0f, 255.0f)));
                    dst[dst_idx + 1] = s_cast<std::byte>(s_cast<u8>(std::clamp(g * 255.0f + 0.5f, 0.0f, 255.0f)));
                    dst[dst_idx + 2] = s_cast<std::byte>(s_cast<u8>(std::clamp(b * 255.0f + 0.5f, 0.0f, 255.0f)));
                }
                dst[dst_idx + 3] = s_cast<std::byte>(s_cast<u8>(std::clamp(pa * 255.0f + 0.5f, 0.0f, 255.0f)));
            }
        }
    }

    auto ImageProcessor::compute_alpha_coverage(const std::byte* data, u32 pixel_count, f32 alpha_ref) -> f32 {
        u32 count = 0;
        u8 ref_byte = s_cast<u8>(alpha_ref * 255.0f);
        for(u32 i = 0; i < pixel_count; ++i) {
            if(s_cast<u8>(data[i * 4 + 3]) > ref_byte) {
                ++count;
            }
        }
        return s_cast<f32>(count) / s_cast<f32>(pixel_count);
    }

    void ImageProcessor::scale_alpha_to_coverage(std::byte* data, u32 pixel_count, f32 desired_coverage, f32 alpha_ref) {
        if(desired_coverage <= 0.0f || desired_coverage >= 1.0f) { return; }
        if(pixel_count == 0) { return; }

        f32 lo = 0.0f, hi = 1.0f;
        for(u32 iter = 0; iter < 16; ++iter) {
            f32 mid = (lo + hi) * 0.5f;
            f32 coverage = compute_alpha_coverage(data, pixel_count, mid);
            if(coverage < desired_coverage) {
                hi = mid;
            } else {
                lo = mid;
            }
        }

        f32 a_r = (lo + hi) * 0.5f;
        if(a_r < 1e-6f) { return; }

        f32 scale = alpha_ref / a_r;

        for(u32 i = 0; i < pixel_count; ++i) {
            f32 a = s_cast<f32>(s_cast<u8>(data[i * 4 + 3])) / 255.0f;
            a = std::clamp(a * scale, 0.0f, 1.0f);
            data[i * 4 + 3] = s_cast<std::byte>(s_cast<u8>(a * 255.0f + 0.5f));
        }
    }

    auto ImageProcessor::generate_mipmaps(LoadedImage& image, bool preserve_alpha_coverage, f32 alpha_ref) -> LoadedImage {
        PROFILE_SCOPE;

        u32 max_dim = std::max(image.width, image.height);
        u32 mip_level_count = s_cast<u32>(std::floor(std::log2(max_dim))) + 1;

        bool is_srgb = (image.format == daxa::Format::R8G8B8A8_SRGB);

        f32 base_coverage = 0.0f;
        if(preserve_alpha_coverage) {
            base_coverage = compute_alpha_coverage(image.data.data(), image.width * image.height, alpha_ref);
        }

        LoadedImage result = {};
        result.format = image.format;
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;

        result.mipmaps.emplace_back(image.data.data(), image.data.size());

        u32 current_width = image.width;
        u32 current_height = image.height;
        std::vector<std::byte> prev_level = image.data;

        for(u32 mip = 1; mip < mip_level_count; ++mip) {
            u32 next_width = std::max(1u, current_width / 2);
            u32 next_height = std::max(1u, current_height / 2);

            std::vector<std::byte> next_level(next_width * next_height * 4);
            if(preserve_alpha_coverage) {
                downsample_rgba8_premultiplied(prev_level.data(), next_level.data(), current_width, current_height, is_srgb);
                scale_alpha_to_coverage(next_level.data(), next_width * next_height, base_coverage, alpha_ref);
            } else {
                downsample_rgba8(prev_level.data(), next_level.data(), current_width, current_height);
            }

            usize prev_size = result.data.size();
            result.data.resize(prev_size + next_level.size());
            std::memcpy(result.data.data() + prev_size, next_level.data(), next_level.size());
            result.mipmaps.emplace_back(result.data.data() + prev_size, next_level.size());

            prev_level = std::move(next_level);
            current_width = next_width;
            current_height = next_height;
        }

        if(result.data.empty()) {
            result.data = image.data;
        } else {
            std::vector<std::byte> full_data;
            full_data.reserve(image.data.size() + result.data.size());
            full_data.insert(full_data.end(), image.data.begin(), image.data.end());
            full_data.insert(full_data.end(), result.data.begin(), result.data.end());
            result.data = std::move(full_data);

            result.mipmaps.clear();
            usize offset = 0;
            current_width = image.width;
            current_height = image.height;
            for(u32 mip = 0; mip < mip_level_count; ++mip) {
                usize mip_size = current_width * current_height * 4;
                result.mipmaps.emplace_back(result.data.data() + offset, mip_size);
                offset += mip_size;
                current_width = std::max(1u, current_width / 2);
                current_height = std::max(1u, current_height / 2);
            }
        }

        return result;
    }

    auto ImageProcessor::compress_to_bc1(LoadedImage& image, bool is_srgb) -> LoadedImage {
        PROFILE_SCOPE;

        initialize_compressor();

        LoadedImage result = {};
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;
        result.format = is_srgb ? daxa::Format::BC1_RGBA_SRGB_BLOCK : daxa::Format::BC1_RGBA_UNORM_BLOCK;

        u32 blocks_x = (image.width + 3) / 4;
        u32 blocks_y = (image.height + 3) / 4;
        usize block_count = blocks_x * blocks_y;
        usize compressed_size = block_count * 8;

        result.data.resize(compressed_size);
        result.mipmaps.emplace_back(result.data.data(), compressed_size);

        const std::byte *src = image.data.data();
        std::byte *dst = result.data.data();

        for(u32 by = 0; by < blocks_y; by++) {
            for(u32 bx = 0; bx < blocks_x; bx++) {
                u8 block_pixels[16 * 4];

                for(u32 y = 0; y < 4; y++) {
                    for(u32 x = 0; x < 4; x++) {
                        u32 src_x = std::min(bx * 4 + x, image.width - 1);
                        u32 src_y = std::min(by * 4 + y, image.height - 1);
                        u32 src_idx = (src_y * image.width + src_x) * 4;
                        u32 dst_idx = (y * 4 + x) * 4;

                        block_pixels[dst_idx + 0] = s_cast<u8>(src[src_idx + 0]);
                        block_pixels[dst_idx + 1] = s_cast<u8>(src[src_idx + 1]);
                        block_pixels[dst_idx + 2] = s_cast<u8>(src[src_idx + 2]);
                        block_pixels[dst_idx + 3] = s_cast<u8>(src[src_idx + 3]);
                    }
                }

                usize block_offset = (by * blocks_x + bx) * 8;
                rgbcx::encode_bc1(10, dst + block_offset, block_pixels, false, false);
            }
        }

        return result;
    }

    auto ImageProcessor::compress_to_bc3(LoadedImage& image, bool is_srgb) -> LoadedImage {
        PROFILE_SCOPE;

        initialize_compressor();

        LoadedImage result = {};
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;
        result.format = is_srgb ? daxa::Format::BC3_SRGB_BLOCK : daxa::Format::BC3_UNORM_BLOCK;

        u32 blocks_x = (image.width + 3) / 4;
        u32 blocks_y = (image.height + 3) / 4;
        usize block_count = blocks_x * blocks_y;
        usize compressed_size = block_count * 16;

        result.data.resize(compressed_size);
        result.mipmaps.emplace_back(result.data.data(), compressed_size);

        const std::byte* src = image.data.data();
        std::byte* dst = result.data.data();

        for(u32 by = 0; by < blocks_y; ++by) {
            for(u32 bx = 0; bx < blocks_x; ++bx) {
                u8 block_pixels[16 * 4];

                for(u32 y = 0; y < 4; y++) {
                    for(u32 x = 0; x < 4; x++) {
                        u32 src_x = std::min(bx * 4 + x, image.width - 1);
                        u32 src_y = std::min(by * 4 + y, image.height - 1);
                        u32 src_idx = (src_y * image.width + src_x) * 4;
                        u32 dst_idx = (y * 4 + x) * 4;

                        block_pixels[dst_idx + 0] = s_cast<u8>(src[src_idx + 0]);
                        block_pixels[dst_idx + 1] = s_cast<u8>(src[src_idx + 1]);
                        block_pixels[dst_idx + 2] = s_cast<u8>(src[src_idx + 2]);
                        block_pixels[dst_idx + 3] = s_cast<u8>(src[src_idx + 3]);
                    }
                }

                usize block_offset = (by * blocks_x + bx) * 16;
                rgbcx::encode_bc3(10, dst + block_offset, block_pixels);
            }
        }

        return result;
    }

    auto ImageProcessor::compress_to_bc5_rg(LoadedImage& image) -> LoadedImage {
        PROFILE_SCOPE;

        initialize_compressor();

        LoadedImage result = {};
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;
        result.format = daxa::Format::BC5_UNORM_BLOCK;

        u32 blocks_x = (image.width + 3) / 4;
        u32 blocks_y = (image.height + 3) / 4;
        usize block_count = blocks_x * blocks_y;
        usize compressed_size = block_count * 16;

        result.data.resize(compressed_size);
        result.mipmaps.push_back(std::span<std::byte const>(result.data.data(), compressed_size));

        const std::byte* src = image.data.data();
        std::byte* dst = result.data.data();

        for(u32 by = 0; by < blocks_y; by++) {
            for(u32 bx = 0; bx < blocks_x; bx++) {
                u8 block_pixels[16 * 4];

                for(u32 y = 0; y < 4; y++) {
                    for(u32 x = 0; x < 4; x++) {
                        u32 src_x = std::min(bx * 4 + x, image.width - 1);
                        u32 src_y = std::min(by * 4 + y, image.height - 1);
                        u32 src_idx = (src_y * image.width + src_x) * 4;
                        u32 dst_idx = (y * 4 + x) * 4;

                        block_pixels[dst_idx + 0] = s_cast<u8>(src[src_idx + 2]);
                        block_pixels[dst_idx + 1] = s_cast<u8>(src[src_idx + 1]);
                        block_pixels[dst_idx + 2] = 0;
                        block_pixels[dst_idx + 3] = 255;
                    }
                }

                usize block_offset = (by * blocks_x + bx) * 16;
                rgbcx::encode_bc5(dst + block_offset, block_pixels, 0, 1, 4);
            }
        }

        return result;
    }

    auto ImageProcessor::compress_to_bc5_normal(LoadedImage& image) -> LoadedImage {
        PROFILE_SCOPE;

        initialize_compressor();

        LoadedImage result = {};
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;
        result.format = daxa::Format::BC5_UNORM_BLOCK;

        u32 blocks_x = (image.width + 3) / 4;
        u32 blocks_y = (image.height + 3) / 4;
        usize block_count = blocks_x * blocks_y;
        usize compressed_size = block_count * 16;

        result.data.resize(compressed_size);
        result.mipmaps.emplace_back(result.data.data(), compressed_size);

        const std::byte* src = image.data.data();
        std::byte* dst = result.data.data();

        for(u32 by = 0; by < blocks_y; by++) {
            for(u32 bx = 0; bx < blocks_x; bx++) {
                u8 block_pixels[16 * 4];

                for(u32 y = 0; y < 4; y++) {
                    for(u32 x = 0; x < 4; x++) {
                        u32 src_x = std::min(bx * 4 + x, image.width - 1);
                        u32 src_y = std::min(by * 4 + y, image.height - 1);
                        u32 src_idx = (src_y * image.width + src_x) * 4;
                        u32 dst_idx = (y * 4 + x) * 4;

                        f32 nx = (s_cast<f32>(src[src_idx + 0]) / 255.0f) * 2.0f - 1.0f;
                        f32 ny = (s_cast<f32>(src[src_idx + 1]) / 255.0f) * 2.0f - 1.0f;
                        f32 nz = (s_cast<f32>(src[src_idx + 2]) / 255.0f) * 2.0f - 1.0f;

                        f32 len = std::sqrt(nx * nx + ny * ny + nz * nz);
                        if(len > 0.0f) {
                            nx /= len;
                            ny /= len;
                        }

                        block_pixels[dst_idx + 0] = s_cast<u8>((nx * 0.5f + 0.5f) * 255.0f);
                        block_pixels[dst_idx + 1] = s_cast<u8>((ny * 0.5f + 0.5f) * 255.0f);
                        block_pixels[dst_idx + 2] = 0;
                        block_pixels[dst_idx + 3] = 255;
                    }
                }

                usize block_offset = (by * blocks_x + bx) * 16;
                rgbcx::encode_bc5(dst + block_offset, block_pixels, 0, 1, 4);
            }
        }

        return result;
    }

    auto ImageProcessor::compress_image(LoadedImage& image, TextureType texture_type) -> LoadedImage {
        PROFILE_SCOPE;

        if(image.format != daxa::Format::R8G8B8A8_UNORM && image.format != daxa::Format::R8G8B8A8_SRGB) {
            return image;
        }

        bool is_srgb = (image.format == daxa::Format::R8G8B8A8_SRGB);
        bool alpha_tested = (texture_type == TextureType::GltfAlbedo) && has_alpha_channel(image);
        LoadedImage image_with_mipmaps = generate_mipmaps(image, alpha_tested);

        LoadedImage result = {};
        result.width = image.width;
        result.height = image.height;
        result.depth = image.depth;

        daxa::Format target_format;
        switch(texture_type) {
            case TextureType::GltfAlbedo: {
                target_format = has_alpha_channel(image)
                                    ? (is_srgb ? daxa::Format::BC3_SRGB_BLOCK : daxa::Format::BC3_UNORM_BLOCK)
                                    : (is_srgb ? daxa::Format::BC1_RGBA_SRGB_BLOCK : daxa::Format::BC1_RGBA_UNORM_BLOCK);
                break;
            }

            case TextureType::GltfEmissive:
                target_format = is_srgb ? daxa::Format::BC1_RGBA_SRGB_BLOCK : daxa::Format::BC1_RGBA_UNORM_BLOCK;
                break;

            case TextureType::GltfRoughnessMetallic:
                target_format = daxa::Format::BC5_UNORM_BLOCK;
                break;

            case TextureType::GltfNormal:
                target_format = daxa::Format::BC5_UNORM_BLOCK;
                break;

            default:
                return image;
        }

        result.format = target_format;

        u32 current_width = image.width;
        u32 current_height = image.height;

        for(u32 mip = 0; mip < image_with_mipmaps.mipmaps.size(); ++mip) {
            LoadedImage mip_image = {};
            mip_image.width = current_width;
            mip_image.height = current_height;
            mip_image.depth = 1;
            mip_image.format = image.format;
            mip_image.data.resize(image_with_mipmaps.mipmaps[mip].size());
            std::memcpy(mip_image.data.data(), image_with_mipmaps.mipmaps[mip].data(), image_with_mipmaps.mipmaps[mip].size());

            LoadedImage compressed_mip;

            switch(texture_type) {
            case TextureType::GltfAlbedo:
                if(has_alpha_channel(mip_image)) {
                    compressed_mip = compress_to_bc3(mip_image, is_srgb);
                } else {
                    compressed_mip = compress_to_bc1(mip_image, is_srgb);
                }
                break;

            case TextureType::GltfEmissive:
                compressed_mip = compress_to_bc1(mip_image, is_srgb);
                break;

            case TextureType::GltfRoughnessMetallic:
                compressed_mip = compress_to_bc5_rg(mip_image);
                break;

            case TextureType::GltfNormal:
                compressed_mip = compress_to_bc5_normal(mip_image);
                break;

            default:
                compressed_mip = mip_image;
                break;
            }

            usize prev_size = result.data.size();
            result.data.resize(prev_size + compressed_mip.data.size());
            std::memcpy(result.data.data() + prev_size, compressed_mip.data.data(), compressed_mip.data.size());
            result.mipmaps.emplace_back(result.data.data() + prev_size, compressed_mip.data.size());

            current_width = std::max(1u, current_width / 2);
            current_height = std::max(1u, current_height / 2);
        }

        return result;
    }
} // namespace foundation
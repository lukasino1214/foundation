#include "asset_processor.hpp"

#include <utils/io.hpp>

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

namespace foundation {
    static auto dds_format_to_daxa_format(DXGI_FORMAT format, bool alpha_flag) -> daxa::Format {
        switch(format) {
            case DXGI_FORMAT_BC1_UNORM: {
                if (alpha_flag) {
                    return daxa::Format::BC1_RGBA_UNORM_BLOCK;
                } else {
                    return daxa::Format::BC1_RGB_UNORM_BLOCK;
                }
            }
            case DXGI_FORMAT_BC1_UNORM_SRGB: {
                if (alpha_flag) {
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
                return daxa::Format::R8G8_SINT;
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

    auto AssetProcessor::load_image(const std::filesystem::path& file_path) -> LoadedImage {
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error(fmt::format("File does not exist with path: {}", file_path.string()));
        }

        std::filesystem::path extension = file_path.extension();
        if(extension == ".dds") {
            return load_dds(file_path);
        } else if(extension == ".png") {
            return load_png(file_path);
        } else if(extension == ".jpg" || extension == ".jpeg") {
            return load_jpeg(file_path);
        } else {
            throw std::runtime_error(fmt::format("Unsupported image format: {}", file_path.string()));
        }
    }

    auto AssetProcessor::load_dds(const std::filesystem::path& file_path) -> LoadedImage {
        dds::Image dds_image = {};
        dds::ReadResult result = dds::readFile(file_path.string(), &dds_image);
        if (result != dds::ReadResult::Success) {
            throw std::runtime_error("something went wrong");
        }

        LoadedImage loaded_image = {};
        loaded_image.format = dds_format_to_daxa_format(dds_image.format, dds_image.supportsAlpha);
        loaded_image.width = dds_image.width;
        loaded_image.height = dds_image.height;
        loaded_image.depth = dds_image.depth;

        usize total_data_size = 0;
        for (const auto& mipmap : dds_image.mipmaps) {
            total_data_size += mipmap.size();
        }
        loaded_image.data.resize(total_data_size);

        usize current_offset = 0;
        loaded_image.mipmaps.reserve(dds_image.mipmaps.size());
        for (const auto& mipmap : dds_image.mipmaps) {
            std::memcpy(loaded_image.data.data() + current_offset, mipmap.data(), mipmap.size());
            loaded_image.mipmaps.emplace_back(loaded_image.data.data() + current_offset, mipmap.size());
            current_offset += mipmap.size();
        }

        return loaded_image;
    }

    #define WORKBUF_STACK_SIZE 8192

    auto AssetProcessor::load_png(const std::filesystem::path& file_path) -> LoadedImage {
        std::vector<std::byte> buffer = io::read_file(file_path);
        LoadedImage loaded_image = {};

        wuffs_png__decoder dec;
        wuffs_base__status status = wuffs_png__decoder__initialize(&dec, sizeof(wuffs_png__decoder), WUFFS_VERSION, 0);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        wuffs_png__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);

        wuffs_base__io_buffer io_buf = wuffs_base__ptr_u8__reader(r_cast<u8*>(buffer.data()), buffer.size(), true);
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
        
        int src_channels;
        if (coloration == WUFFS_BASE__PIXEL_COLORATION__GRAY) {
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 1 : 2;
        } else { // RICH
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 3 : 4;
        }
        
        int desired_channels = 4;
        // if (channels_in_file) *channels_in_file = src_channels;

        int n = desired_channels ? desired_channels : src_channels;
        uint32_t dst_pfmt;
        bool is_16bit = false;
        
        if(is_16bit) {
            dst_pfmt = (n == 1) ? WUFFS_BASE__PIXEL_FORMAT__Y_16LE : WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE;
            if (n == 3) n = 4; // Force 4 channels for RGB -> RGBA conversion in 16-bit mode
        } else {
            switch (n) {
                case 1:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__Y; break;
                case 2:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL; break;
                case 3:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGB; break;
                default: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL; break;
            }
        }

        wuffs_base__pixel_config__set(&config.pixcfg, dst_pfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);
        size_t bytes_per_pixel = (size_t) is_16bit ? (n + n) : n;

        if (width == 0 || height == 0 || bytes_per_pixel == 0 ||
            (size_t)width > SIZE_MAX / height ||
            (size_t)width * height > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        size_t pixel_buf_size = (size_t)width * (size_t)height * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_png__decoder__workbuf_len(&dec);
        void* workbuf_ptr = NULL;

        wuffs_base__slice_u8 workbuf = {};

        if (workbuf_len_range.max_incl > 0) {
            size_t wb_len = (size_t)workbuf_len_range.max_incl;
            if (wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            uint8_t workbuf_stack[WORKBUF_STACK_SIZE];
            if (wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if (!workbuf_ptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = (uint8_t*)workbuf_ptr;
                workbuf.len = wb_len;
            }
        }

        wuffs_base__pixel_buffer pb = {0};
        status = wuffs_base__pixel_buffer__set_from_slice(&pb, &config.pixcfg, wuffs_base__make_slice_u8(r_cast<u8*>(loaded_image.data.data()), pixel_buf_size));
        if (!wuffs_base__status__is_ok(&status)) {
            if (workbuf_ptr) free(workbuf_ptr);
            throw std::runtime_error("something went wrong");
        }

        status = wuffs_png__decoder__decode_frame(&dec, &pb, &io_buf, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);
        
        if (workbuf_ptr) free(workbuf_ptr);

        if (!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        return loaded_image;
    }

    auto AssetProcessor::load_jpeg(const std::filesystem::path& file_path) -> LoadedImage {
        std::vector<std::byte> buffer = io::read_file(file_path);
        LoadedImage loaded_image = {};
    
        wuffs_jpeg__decoder dec;
        wuffs_base__status status = wuffs_jpeg__decoder__initialize(&dec, sizeof(wuffs_jpeg__decoder), WUFFS_VERSION, 0);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        wuffs_jpeg__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);

        wuffs_base__io_buffer io_buf = wuffs_base__ptr_u8__reader(r_cast<u8*>(buffer.data()), buffer.size(), true);
        wuffs_base__image_config config = {0};
        
        status = wuffs_jpeg__decoder__decode_image_config(&dec, &config, &io_buf);
        if (!wuffs_base__status__is_ok(&status)) {
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
        
        int src_channels;
        if (coloration == WUFFS_BASE__PIXEL_COLORATION__GRAY) {
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 1 : 2;
        } else { // RICH
            src_channels = (transparency == WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE) ? 3 : 4;
        }
        
        int desired_channels = 4;
        // if (channels_in_file) *channels_in_file = src_channels;

        int n = desired_channels ? desired_channels : src_channels;
        uint32_t dst_pfmt;
        
        switch (n) {
            case 1:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__Y; break;
            case 2:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL; break;
            case 3:  dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGB; break;
            default: dst_pfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL; break;
        }

        wuffs_base__pixel_config__set(&config.pixcfg, dst_pfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);

        size_t bytes_per_pixel = (size_t)n;

        if (width == 0 || height == 0 || bytes_per_pixel == 0 ||
            (size_t)width > SIZE_MAX / height ||
            (size_t)width * height > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        size_t pixel_buf_size = (size_t)width * (size_t)height * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_jpeg__decoder__workbuf_len(&dec);
        void* workbuf_ptr = NULL;

        wuffs_base__slice_u8 workbuf = {0};

        if (workbuf_len_range.max_incl > 0) {
            size_t wb_len = (size_t)workbuf_len_range.max_incl;
            if (wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            uint8_t workbuf_stack[WORKBUF_STACK_SIZE];
            if (wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if (!workbuf_ptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = (uint8_t*)workbuf_ptr;
                workbuf.len = wb_len;
            }
        }

        wuffs_base__pixel_buffer pb = {0};
        status = wuffs_base__pixel_buffer__set_from_slice(&pb, &config.pixcfg, wuffs_base__make_slice_u8(r_cast<u8*>(loaded_image.data.data()), pixel_buf_size));
        if (!wuffs_base__status__is_ok(&status)) {
            if (workbuf_ptr) free(workbuf_ptr);
            throw std::runtime_error("something went wrong");
        }

        status = wuffs_jpeg__decoder__decode_frame(&dec, &pb, &io_buf, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);
        
        if (workbuf_ptr) free(workbuf_ptr);

        if (!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        return loaded_image;
    }
}
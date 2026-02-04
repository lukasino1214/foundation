#include "asset_processor.hpp"

#include <utils/io.hpp>
#include <graphics/gpu_context.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wold-style-cast"

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

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#pragma clang diagnostic pop

#include <meshoptimizer.h>

namespace foundation {
    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto load_accessor_data_from_file(const std::filesystem::path& root_path, const fastgltf::Asset& gltf_asset, const fastgltf::Accessor& accesor) -> std::vector<ElemT> {
        PROFILE_SCOPE;
        
        static_assert(!IS_INDEX_BUFFER || std::is_same_v<ElemT, u32>, "Index Buffer must be u32");
        fastgltf::BufferView const & gltf_buffer_view = gltf_asset.bufferViews.at(accesor.bufferViewIndex.value());
        fastgltf::Buffer const & gltf_buffer = gltf_asset.buffers.at(gltf_buffer_view.bufferIndex);
        if (!std::holds_alternative<fastgltf::sources::URI>(gltf_buffer.data)) {}

        fastgltf::sources::URI uri = std::get<fastgltf::sources::URI>(gltf_buffer.data);
        std::filesystem::path const full_buffer_path = root_path / uri.uri.fspath();
        std::ifstream ifs{full_buffer_path, std::ios::binary};
        if (!ifs) {}

        ifs.seekg(s_cast<std::streamsize>(gltf_buffer_view.byteOffset + accesor.byteOffset + uri.fileByteOffset));
        std::vector<u16> raw = {};
        size_t const elem_byte_size = fastgltf::getElementByteSize(accesor.type, accesor.componentType);
        raw.resize((accesor.count * elem_byte_size) / 2);
        if (!ifs.read(r_cast<char *>(raw.data()), s_cast<std::streamsize>(accesor.count * elem_byte_size))) {}

        auto buffer_adapter = [&](fastgltf::Asset const & /* asset */, u32 /* asset_index */) -> fastgltf::span<std::byte const> {
            return fastgltf::span<std::byte const>(reinterpret_cast<std::byte const *>(raw.data()) - accesor.byteOffset, accesor.count * elem_byte_size);
        };

        std::vector<ElemT> ret(accesor.count);
        if constexpr (IS_INDEX_BUFFER) {
            if (accesor.componentType == fastgltf::ComponentType::UnsignedShort) {
                std::vector<u16> u16_index_buffer(accesor.count);
                fastgltf::copyFromAccessor<u16>(gltf_asset, accesor, u16_index_buffer.data(), buffer_adapter);
                for (size_t i = 0; i < u16_index_buffer.size(); ++i) {
                    ret[i] = s_cast<u32>(u16_index_buffer[i]);
                }
            } else {
                fastgltf::copyFromAccessor<u32>(gltf_asset, accesor, ret.data(), buffer_adapter);
            }
        } else {
            fastgltf::copyFromAccessor<ElemT>(gltf_asset, accesor, ret.data(), buffer_adapter);
        }
        return ret;
    }

    AssetProcessor::AssetProcessor(GPUContext* _gpu_context) : gpu_context{_gpu_context} {}
    AssetProcessor::~AssetProcessor() {}

    auto AssetProcessor::generate_index_buffer(const GenerateIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        PROFILE_SCOPE;

        std::vector<meshopt_Stream> streams = {};
        streams.push_back(meshopt_Stream { .data = info.unindexed_positions.data(), .size = info.unindexed_positions.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_normals.data(), .size = info.unindexed_normals.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_uvs.data(), .size = info.unindexed_uvs.size(), .stride = sizeof(f32vec2) });
        
        usize index_count = info.unindexed_positions.size();
        usize unindexed_vertex_count = info.unindexed_positions.size();
        
        std::vector<u32> remap_table(unindexed_vertex_count);
        usize unique_vertex_count = meshopt_generateVertexRemapMulti(remap_table.data(), nullptr, index_count, unindexed_vertex_count, streams.data(), streams.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.indices.resize(index_count);

        meshopt_remapVertexBuffer(ret.positions.data(), info.unindexed_positions.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), info.unindexed_normals.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), info.unindexed_uvs.data(), unindexed_vertex_count, sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.indices.data(), nullptr, index_count, remap_table.data());

        return ret;
    }

    auto AssetProcessor::optimize_vertex_cache(const std::vector<u32>& index_buffer, u32 vertex_count) -> std::vector<u32> {
        PROFILE_SCOPE;

        std::vector<u32> optimized_index_buffer(index_buffer.size());
        meshopt_optimizeVertexCache(optimized_index_buffer.data(), index_buffer.data(), index_buffer.size(), vertex_count);
        return optimized_index_buffer;
    }

    auto AssetProcessor::optimize_vertex_fetch(const ProcessedIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        PROFILE_SCOPE;

        std::vector<u32> remap_table(info.positions.size());
        usize unique_vertex_count = meshopt_optimizeVertexFetchRemap(remap_table.data(), info.indices.data(), info.indices.size(), info.positions.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.indices.resize(info.indices.size());

        meshopt_remapVertexBuffer(ret.positions.data(), info.positions.data(), info.positions.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), info.normals.data(), info.normals.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), info.uvs.data(), info.uvs.size(), sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.indices.data(), info.indices.data(), info.indices.size(), remap_table.data());

        return ret;
    }

    auto AssetProcessor::load_mesh(fastgltf::Asset* asset, const std::filesystem::path& file_path, u32 mesh_group_index, u32 mesh_index) -> LoadedMesh {
        PROFILE_SCOPE;

        const fastgltf::Mesh& gltf_mesh = asset->meshes[mesh_group_index];
        const fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[mesh_index];
        if(gltf_primitive.type != fastgltf::PrimitiveType::Triangles) {
            throw std::runtime_error("expecting primitive type of triangles");
        }

        auto fill = [](auto& vec, u32 required_size = 0) {
            if(vec.empty()) {
                vec.resize(required_size, {});
            }
        };

        LoadedMesh loaded_mesh = {};

        const fastgltf::Attribute* position_attribute_iter = gltf_primitive.findAttribute("POSITION");
        if(position_attribute_iter != gltf_primitive.attributes.end()) {
            loaded_mesh.positions = load_accessor_data_from_file<glm::vec3, false>(file_path.parent_path(), *asset, asset->accessors[position_attribute_iter->accessorIndex]);
        }

        u32 vertex_count = s_cast<u32>(loaded_mesh.positions.size());

        const fastgltf::Attribute* normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
        if(normal_attribute_iter != gltf_primitive.attributes.end()) {
            loaded_mesh.normals = load_accessor_data_from_file<glm::vec3, false>(file_path.parent_path(), *asset, asset->accessors[normal_attribute_iter->accessorIndex]);
        } else {
            fill(loaded_mesh.normals, vertex_count);
        }

        const fastgltf::Attribute* uv_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
        if(uv_attribute_iter != gltf_primitive.attributes.end()) {
            loaded_mesh.uvs = load_accessor_data_from_file<glm::vec2, false>(file_path.parent_path(), *asset, asset->accessors[uv_attribute_iter->accessorIndex]);
        } else {
            fill(loaded_mesh.uvs, vertex_count);
        }

        if(gltf_primitive.indicesAccessor.has_value()) {
            loaded_mesh.indices = load_accessor_data_from_file<u32, true>(file_path.parent_path(), *asset, asset->accessors[gltf_primitive.indicesAccessor.value()]);
        }

        auto& positions = loaded_mesh.positions;
        auto& normals = loaded_mesh.normals;
        auto& uvs = loaded_mesh.uvs;
        auto& indices = loaded_mesh.indices;

        if(loaded_mesh.indices.empty()) {
            ProcessedIndexBufferInfo ret = generate_index_buffer(GenerateIndexBufferInfo {
                .unindexed_positions = positions,
                .unindexed_normals = normals,
                .unindexed_uvs = uvs
            });

            positions = ret.positions;
            normals = ret.normals;
            uvs = ret.uvs;
            indices = ret.indices;
        }

        {
            ProcessedIndexBufferInfo ret = optimize_vertex_fetch(ProcessedIndexBufferInfo {
                .positions = positions,
                .normals = normals,
                .uvs = uvs,
                .indices = indices
            });

            positions = ret.positions;
            normals = ret.normals;
            uvs = ret.uvs;
            indices = ret.indices;
        }

        indices = optimize_vertex_cache(indices, vertex_count);

        return loaded_mesh;
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

    auto AssetProcessor::load_image(const std::filesystem::path& file_path) -> LoadedImage {
        PROFILE_SCOPE;
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
        PROFILE_SCOPE;
        dds::Image dds_image = {};
        dds::ReadResult result = dds::readFile(file_path.string(), &dds_image);
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

    auto AssetProcessor::load_png(const std::filesystem::path& file_path) -> LoadedImage {
        PROFILE_SCOPE;

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
        size_t bytes_per_pixel = s_cast<size_t>(is_16bit ? (n + n) : n);

        if(width == 0 || height == 0 || bytes_per_pixel == 0 ||
           s_cast<size_t>(width) > SIZE_MAX / height ||
           s_cast<size_t>(width * height) > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        size_t pixel_buf_size = s_cast<size_t>(width * height) * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_png__decoder__workbuf_len(&dec);
        void* workbuf_ptr = nullptr;

        wuffs_base__slice_u8 workbuf = {};

        if(workbuf_len_range.max_incl > 0) {
            size_t wb_len = s_cast<size_t>(workbuf_len_range.max_incl);
            if(wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            uint8_t workbuf_stack[WORKBUF_STACK_SIZE];
            if(wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if(!workbuf_ptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = s_cast<uint8_t *>(workbuf_ptr);
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

    auto AssetProcessor::load_jpeg(const std::filesystem::path& file_path) -> LoadedImage {
        PROFILE_SCOPE;

        std::vector<std::byte> buffer = io::read_file(file_path);
        LoadedImage loaded_image = {};

        wuffs_jpeg__decoder dec;
        wuffs_base__status status = wuffs_jpeg__decoder__initialize(&dec, sizeof(wuffs_jpeg__decoder), WUFFS_VERSION, 0);
        if(!wuffs_base__status__is_ok(&status)) {
            throw std::runtime_error("something went wrong");
        }

        wuffs_jpeg__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);

        wuffs_base__io_buffer io_buf = wuffs_base__ptr_u8__reader(r_cast<u8*>(buffer.data()), buffer.size(), true);
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

        size_t bytes_per_pixel = s_cast<size_t>(n);

        if(width == 0 || height == 0 || bytes_per_pixel == 0 ||
           s_cast<size_t>(width) > SIZE_MAX / height ||
           s_cast<size_t>(width * height) > SIZE_MAX / bytes_per_pixel) {
            throw std::runtime_error("something went wrong");
        }

        size_t pixel_buf_size = s_cast<size_t>(width * height) * bytes_per_pixel;
        loaded_image.data.resize(pixel_buf_size);

        wuffs_base__range_ii_u64 workbuf_len_range = wuffs_jpeg__decoder__workbuf_len(&dec);
        void* workbuf_ptr = nullptr;

        wuffs_base__slice_u8 workbuf = {};

        if(workbuf_len_range.max_incl > 0) {
            size_t wb_len = s_cast<size_t>(workbuf_len_range.max_incl);
            if(wb_len > SIZE_MAX) {
                throw std::runtime_error("something went wrong");
            }

            uint8_t workbuf_stack[WORKBUF_STACK_SIZE];
            if(wb_len <= WORKBUF_STACK_SIZE) {
                workbuf.ptr = workbuf_stack;
                workbuf.len = wb_len;
            } else {
                workbuf_ptr = malloc(wb_len);
                if(workbuf_ptr == nullptr) {
                    throw std::runtime_error("something went wrong");
                }
                workbuf.ptr = s_cast<uint8_t*>(workbuf_ptr);
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

    void AssetProcessor::process_mesh_load(const LoadMeshInfo& info) {
        PROFILE_SCOPE;

        LoadedMesh loaded_mesh = AssetProcessor::load_mesh(info.asset, info.asset_path, info.asset_local_mesh_group_index, info.asset_local_mesh_index);

        auto& positions = loaded_mesh.positions;
        auto& normals = loaded_mesh.normals;
        auto& uvs = loaded_mesh.uvs;
        auto& indices = loaded_mesh.indices;
        
        MeshGeometryData mesh_geometry_data = {};
        mesh_geometry_data.vertex_count = s_cast<u32>(positions.size());
        mesh_geometry_data.index_count = s_cast<u32>(indices.size());
        
        usize total_mesh_size = positions.size() * sizeof(glm::vec3) + normals.size() * sizeof(glm::vec3) + uvs.size() * sizeof(glm::vec2);
        
        const fastgltf::Mesh& gltf_mesh = info.asset->meshes[info.asset_local_mesh_group_index];
        std::string mesh_name = fmt::format("mesh {}", gltf_mesh.name.empty() ? fmt::format("{}_{}", info.asset_manifest_index, info.mesh_manifest_index).c_str() : gltf_mesh.name.c_str());

        daxa::BufferId staging_mesh_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
            .size = total_mesh_size,
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = mesh_name.c_str()
        });

        daxa::BufferId mesh_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
            .size = total_mesh_size,
            .name = mesh_name.c_str()
        });

        std::byte* mapped_ptr = gpu_context->device.buffer_host_address(staging_mesh_buffer).value();
        daxa::DeviceAddress bda = gpu_context->device.buffer_device_address(mesh_buffer).value();

        std::memcpy(mapped_ptr, positions.data(), positions.size() * sizeof(glm::vec3));
        mapped_ptr += positions.size() * sizeof(glm::vec3);
        mesh_geometry_data.positions = bda;
        bda += positions.size() * sizeof(glm::vec3);

        std::memcpy(mapped_ptr, normals.data(), normals.size() * sizeof(glm::vec3));
        mapped_ptr += normals.size() * sizeof(glm::vec3);
        mesh_geometry_data.normals = bda;
        bda += normals.size() * sizeof(glm::vec3);

        if(!uvs.empty()) {
            std::memcpy(mapped_ptr, uvs.data(), uvs.size() * sizeof(glm::vec2));
            mapped_ptr += uvs.size() * sizeof(glm::vec2);
            mesh_geometry_data.uvs = bda;
            bda += uvs.size() * sizeof(glm::vec2);
        } else {
            mesh_geometry_data.uvs = 0;
        }

        daxa::BufferId staging_index_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
            .size = indices.size() * sizeof(u32),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = mesh_name.c_str()
        });

        daxa::BufferId index_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
            .size = indices.size() * sizeof(u32),
            .name = mesh_name.c_str()
        });

        std::memcpy(gpu_context->device.buffer_host_address(staging_index_buffer).value(), indices.data(), sizeof(u32) * indices.size());
        mesh_geometry_data.indices = gpu_context->device.buffer_device_address(index_buffer).value();

        {
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            mesh_upload_queue.push_back(MeshUploadInfo {
                .staging_mesh_buffer = staging_mesh_buffer,
                .mesh_buffer = mesh_buffer,
                .staging_index_buffer = staging_index_buffer,
                .index_buffer = index_buffer,
                .mesh_geometry_data = mesh_geometry_data,
                .asset_manifest_index = info.asset_manifest_index,
                .mesh_manifest_index = info.mesh_manifest_index,
                .has_alpha = info.has_alpha,
            });
        }
    }

    void AssetProcessor::process_texture_load(const LoadTextureInfo& info) {
        PROFILE_SCOPE;

        const fastgltf::Image& gltf_image = info.asset->images[info.texture_manifest_index];
        const auto* uri = std::get_if<fastgltf::sources::URI>(&gltf_image.data);
        if(uri == nullptr) { throw std::runtime_error("huh"); }
        LoadedImage loaded_image = AssetProcessor::load_image(info.asset_path.parent_path() / uri->uri.fspath());

        if(info.load_as_srgb) {
            switch(loaded_image.format) {
                case daxa::Format::R8G8B8A8_UNORM: {
                    loaded_image.format = daxa::Format::R8G8B8A8_SRGB;
                    break;
                }
                case daxa::Format::BC1_RGB_UNORM_BLOCK: {
                    loaded_image.format = daxa::Format::BC1_RGB_SRGB_BLOCK;
                    break;
                }
                case daxa::Format::BC1_RGBA_UNORM_BLOCK: {
                    loaded_image.format = daxa::Format::BC1_RGBA_SRGB_BLOCK;
                    break;
                }
                default: {
                    // throw std::runtime_error("Unsupported format");
                    break;
                }
            }
        }

        daxa::BufferId staging_image_buffer = gpu_context->device.create_buffer({
            .size = loaded_image.data.size(),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = fmt::format("staging_texture_{}", gltf_image.name),
        });
        std::memcpy(gpu_context->device.buffer_host_address(staging_image_buffer).value(), loaded_image.data.data(), loaded_image.data.size());

        u32 mip_level_count = loaded_image.mipmaps.empty() ? s_cast<u32>(std::floor(std::log2(std::max(loaded_image.width, loaded_image.height)))) + 1 : s_cast<u32>(loaded_image.mipmaps.size());
        daxa::ImageId image = gpu_context->device.create_image({
            .format = loaded_image.format,
            .size = { loaded_image.width, loaded_image.height, loaded_image.depth },
            .mip_level_count = mip_level_count,
            .array_layer_count = 1,
            .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
            .name = gltf_image.name.c_str(),
        });

        daxa::SamplerId sampler = gpu_context->get_sampler(daxa::SamplerInfo {
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::REPEAT,
            .address_mode_v = daxa::SamplerAddressMode::REPEAT,
            .address_mode_w = daxa::SamplerAddressMode::REPEAT,
            .mip_lod_bias = 0.0f,
            .enable_anisotropy = true,
            .max_anisotropy = 16.0f,
            .enable_compare = false,
            .compare_op = daxa::CompareOp::ALWAYS,
            .min_lod = 0.0f,
            .max_lod = s_cast<f32>(mip_level_count),
            .enable_unnormalized_coordinates = false,
            .name = gltf_image.name.c_str(),
        });

        std::vector<std::pair<u32, u32>> mipmaps = {};
        mipmaps.reserve(loaded_image.mipmaps.size());
        usize current_offset = 0;
        for(u32 i = 0; i < loaded_image.mipmaps.size(); ++i) {
            mipmaps.emplace_back(s_cast<u32>(current_offset), s_cast<u32>(loaded_image.mipmaps[i].size()));
            current_offset += loaded_image.mipmaps[i].size();
        }

        {
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            texture_upload_queue.push_back(TextureUploadInfo {
                .staging_image_buffer = staging_image_buffer,
                .image = image,
                .sampler = sampler,
                .mipmaps = mipmaps,
                .asset_manifest_index = info.asset_manifest_index,
                .texture_manifest_index = info.texture_manifest_index,
            });
        }
    }
    
    auto AssetProcessor::upload_resources(daxa::CommandRecorder& cmd_recorder) -> ResourceUploadInfos {
        PROFILE_SCOPE;

        ResourceUploadInfos ret = {};

        {
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            ret.meshes = std::move(mesh_upload_queue);
            mesh_upload_queue = {};
        }

        if(!ret.meshes.empty()) {
            daxa::BufferId staging_transform_buffer = gpu_context->device.create_buffer({
                .size = sizeof(daxa_f32mat3x4),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging transform buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_transform_buffer);

            daxa::BufferId transform_buffer =  gpu_context->device.create_buffer({
                .size = sizeof(daxa_f32mat3x4),
                .name = "transform buffer",
            });
            cmd_recorder.destroy_buffer_deferred(transform_buffer);

            *gpu_context->device.buffer_host_address_as<f32mat3x4>(staging_transform_buffer).value() = f32mat3x4{
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
            };

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = staging_transform_buffer,
                .dst_buffer = transform_buffer,
                .size = sizeof(daxa_f32mat3x4)
            });

            cmd_recorder.pipeline_barrier(daxa::BarrierInfo {
                .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                .dst_access = daxa::AccessConsts::READ,
            });

            const u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256;
            auto round_up_to_multiple = [](u32 value, u32 multiple_of) -> u32 {
                return ((value + multiple_of - 1) / multiple_of) * multiple_of;
            };

            std::vector<daxa::BlasBuildInfo> blas_build_infos = {};
            std::vector<daxa::BlasTriangleGeometryInfo> geometry_store = {};
            geometry_store.reserve(ret.meshes.size());

            u32 current_scratch_buffer_offset = 0;
            for(MeshUploadInfo& mesh_upload_info : ret.meshes) {
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = mesh_upload_info.staging_mesh_buffer,
                    .dst_buffer = mesh_upload_info.mesh_buffer,
                    .size = gpu_context->device.buffer_info(mesh_upload_info.staging_mesh_buffer).value().size,
                });
                cmd_recorder.destroy_buffer_deferred(mesh_upload_info.staging_mesh_buffer);
    
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = mesh_upload_info.staging_index_buffer,
                    .dst_buffer = mesh_upload_info.index_buffer,
                    .size = gpu_context->device.buffer_info(mesh_upload_info.staging_index_buffer).value().size,
                });
                cmd_recorder.destroy_buffer_deferred(mesh_upload_info.staging_index_buffer);

                geometry_store.push_back(daxa::BlasTriangleGeometryInfo {
                    .vertex_format = daxa::Format::R32G32B32_SFLOAT,
                    .vertex_data = mesh_upload_info.mesh_geometry_data.positions,
                    .vertex_stride = sizeof(daxa_f32vec3),
                    .max_vertex = mesh_upload_info.mesh_geometry_data.vertex_count - 1,
                    .index_type = daxa::IndexType::uint32,
                    .index_data = mesh_upload_info.mesh_geometry_data.indices,
                    .transform_data = gpu_context->device.buffer_device_address(transform_buffer).value(),
                    .count = mesh_upload_info.mesh_geometry_data.index_count / 3,
                    .flags = mesh_upload_info.has_alpha ? daxa::GeometryFlagBits::NONE : daxa::GeometryFlagBits::OPAQUE,
                });

                daxa::BlasBuildInfo blas_build_info = {
                    .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_TRACE | daxa::AccelerationStructureBuildFlagBits::ALLOW_DATA_ACCESS,
                    .geometries = std::span(&geometry_store.back(), 1), 
                    .scratch_data = {},
                };
                
                daxa::AccelerationStructureBuildSizesInfo build_size_info = gpu_context->device.blas_build_sizes(blas_build_info);

                mesh_upload_info.blas = gpu_context->device.create_blas({
                    .size = round_up_to_multiple(s_cast<u32>(build_size_info.acceleration_structure_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT),
                    .name = fmt::format("{} blas", gpu_context->device.buffer_info(mesh_upload_info.mesh_buffer).value().name.c_str())
                });

                u32 aligned_scratch_size = round_up_to_multiple(s_cast<u32>(build_size_info.build_scratch_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT);
                blas_build_info.dst_blas = mesh_upload_info.blas;
                blas_build_info.scratch_data = current_scratch_buffer_offset;
                current_scratch_buffer_offset += aligned_scratch_size;

                blas_build_infos.push_back(blas_build_info);
            }

            daxa::BufferId blas_scratch_buffer = gpu_context->device.create_buffer({
                .size = current_scratch_buffer_offset,
            });
            cmd_recorder.destroy_buffer_deferred(blas_scratch_buffer);

            for(auto& blas_build_info : blas_build_infos) {
                blas_build_info.scratch_data += gpu_context->device.buffer_device_address(blas_scratch_buffer).value();
            }

            cmd_recorder.pipeline_barrier(daxa::BarrierInfo {
                .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                .dst_access = daxa::AccessConsts::READ,
            });

            cmd_recorder.build_acceleration_structures({
                .blas_build_infos = blas_build_infos,
            });
        }

        {
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            ret.textures = std::move(texture_upload_queue);
            texture_upload_queue = {};
        }

        for(const TextureUploadInfo& texture_upload_info : ret.textures) {
            cmd_recorder.destroy_buffer_deferred(texture_upload_info.staging_image_buffer);

            cmd_recorder.pipeline_image_barrier(daxa::ImageBarrierInfo {
                .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
                .image_id = texture_upload_info.image,
                .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
            });

            if(texture_upload_info.mipmaps.empty()) {
                daxa::Extent3D mip_size = gpu_context->device.image_info(texture_upload_info.image).value().size;
                u32 mip_level_count = gpu_context->device.image_info(texture_upload_info.image).value().mip_level_count;

                cmd_recorder.copy_buffer_to_image({
                    .buffer = texture_upload_info.staging_image_buffer,
                    .buffer_offset = 0,
                    .image = texture_upload_info.image,
                    .image_slice = {
                        .mip_level = 0,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .image_offset = { 0, 0, 0 },
                    .image_extent = { mip_size.x, mip_size.y, mip_size.z }
                });

                cmd_recorder.pipeline_image_barrier(daxa::ImageBarrierInfo{
                    .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .dst_access = daxa::AccessConsts::BLIT_READ,
                    .image_id = texture_upload_info.image,
                });

                for(u32 i = 1; i < mip_level_count; i++) {
                    daxa::Extent3D next_mip_size = {
                        std::max<u32>(1, mip_size.x / 2),
                        std::max<u32>(1, mip_size.y / 2),
                        std::max<u32>(1, mip_size.z / 2),
                    };

                    cmd_recorder.blit_image_to_image({
                        .src_image = texture_upload_info.image,
                        .dst_image = texture_upload_info.image,
                        .src_slice = {
                            .mip_level = i - 1,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .src_offsets = {{{0, 0, 0}, {s_cast<i32>(mip_size.x), s_cast<i32>(mip_size.y), s_cast<i32>(mip_size.z)}}},
                        .dst_slice = {
                            .mip_level = i,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .dst_offsets = {{{0, 0, 0}, {s_cast<i32>(next_mip_size.x), s_cast<i32>(next_mip_size.y), s_cast<i32>(next_mip_size.z)}}},
                        .filter = daxa::Filter::LINEAR,
                    });

                    cmd_recorder.pipeline_image_barrier(daxa::ImageBarrierInfo{
                        .src_access = daxa::AccessConsts::BLIT_WRITE,
                        .dst_access = daxa::AccessConsts::BLIT_READ,
                        .image_id = texture_upload_info.image,
                    });
                    
                    mip_size = next_mip_size;
                }
            } else {
                usize staging_offset = 0;
                for(u32 mip_level = 0; mip_level < texture_upload_info.mipmaps.size(); ++mip_level) {
                    const auto& mipmap = texture_upload_info.mipmaps[mip_level];
                    daxa::Extent3D mip_size = gpu_context->device.image_info(texture_upload_info.image).value().size;
                    
                    cmd_recorder.copy_buffer_to_image({
                        .buffer = texture_upload_info.staging_image_buffer,
                        .buffer_offset = staging_offset,
                        .image = texture_upload_info.image,
                        .image_slice = {
                            .mip_level = mip_level,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_offset = {0, 0, 0},
                        .image_extent = {
                            std::max(1u, mip_size.x >> mip_level),
                            std::max(1u, mip_size.y >> mip_level),
                            std::max(1u, mip_size.z >> mip_level),
                        },
                    });

                    staging_offset += mipmap.second;
                }
            }
        }

        return ret;
    }
} // namespace foundation
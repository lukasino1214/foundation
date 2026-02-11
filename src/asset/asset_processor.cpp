#include "asset_processor.hpp"

#include "image_processor.hpp"
#include "mesh_processor.hpp"

#include <utils/io.hpp>
#include <graphics/gpu_context.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wold-style-cast"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#pragma clang diagnostic pop

namespace foundation {
    template auto AssetProcessor::load_accessor_data_from_file<glm::vec2, false>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<glm::vec2>;
    template auto AssetProcessor::load_accessor_data_from_file<glm::vec3, false>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<glm::vec3>;
    template auto AssetProcessor::load_accessor_data_from_file<glm::vec4, false>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<glm::vec4>;
    template auto AssetProcessor::load_accessor_data_from_file<glm::mat4, false>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<glm::mat4>;
    template auto AssetProcessor::load_accessor_data_from_file<f32, false>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<f32>;
    template auto AssetProcessor::load_accessor_data_from_file<u32, true>(const std::filesystem::path&, const fastgltf::Asset&, const fastgltf::Accessor&) -> std::vector<u32>;

    struct BufferData {
        std::vector<std::byte> loaded_data = {};
        std::span<std::byte const> data_span = {};
    };

    auto fastgltf_read_data(const std::filesystem::path& root_path, const fastgltf::Asset& gltf_asset, const fastgltf::DataSource& data, usize total_bytes = 0, usize byte_offset = 0, usize accessor_offset = 0) -> BufferData {
        BufferData ret;

        std::visit(fastgltf::visitor {
            [&](const std::monostate&) {
                throw std::runtime_error("Buffer data is monostate (uninitialized)");
            },
            [&](const fastgltf::sources::BufferView& view) {
                const auto& src_buffer_view = gltf_asset.bufferViews[view.bufferViewIndex];
                const auto& src_buffer = gltf_asset.buffers[src_buffer_view.bufferIndex];
                usize effective_size = total_bytes > 0 ? total_bytes : src_buffer_view.byteLength;

                std::visit(fastgltf::visitor{
                    [&](const fastgltf::sources::Array& arr) {
                        ret.data_span = std::span<std::byte const>(
                            reinterpret_cast<std::byte const*>(arr.bytes.data()) + src_buffer_view.byteOffset + accessor_offset,
                            effective_size
                        );
                    },
                    [&](const fastgltf::sources::Vector& vec) {
                        ret.data_span = std::span<std::byte const>(
                            reinterpret_cast<std::byte const*>(vec.bytes.data()) + src_buffer_view.byteOffset + accessor_offset,
                            effective_size
                        );
                    },
                    [&](const auto&) {
                        throw std::runtime_error("Nested BufferView or unsupported source in BufferView");
                    }
                }, src_buffer.data);
            },
            [&](const fastgltf::sources::URI& uri) {
                const std::filesystem::path full_path = root_path / uri.uri.fspath();
                std::ifstream file(full_path, std::ios::binary);
                if (!file) {
                    throw std::runtime_error("Failed to open buffer file: " + full_path.string());
                }

                const usize file_offset = byte_offset + uri.fileByteOffset;
                file.seekg(static_cast<std::streamsize>(file_offset));

                ret.loaded_data.resize(total_bytes);
                if (!file.read(reinterpret_cast<char*>(ret.loaded_data.data()), static_cast<std::streamsize>(total_bytes))) {
                    throw std::runtime_error("Failed to read buffer data from file");
                }

                ret.data_span = std::span<std::byte const>(ret.loaded_data.data(), total_bytes);
            },
            [&](const fastgltf::sources::Array& arr) {
                usize effective_size = total_bytes > 0 ? total_bytes : arr.bytes.size() - byte_offset;
                ret.data_span = std::span<std::byte const>(reinterpret_cast<std::byte const*>(arr.bytes.data()) + byte_offset, effective_size);
            },
            [&](const fastgltf::sources::Vector& vec) {
                usize effective_size = total_bytes > 0 ? total_bytes : vec.bytes.size() - byte_offset;
                ret.data_span = std::span<std::byte const>(vec.bytes.data() + byte_offset, effective_size);
            },
            [&](const fastgltf::sources::CustomBuffer& /* custom */) {
                throw std::runtime_error("Buffer data is custoom");
            },
            [&](const fastgltf::sources::ByteView& view) {
                usize effective_size = total_bytes > 0 ? total_bytes : view.bytes.size() - byte_offset;
                ret.data_span = std::span<std::byte const>(view.bytes.data() + byte_offset, effective_size);
            },
            [&](const fastgltf::sources::Fallback&) {
                throw std::runtime_error("Fallback buffer source not supported");
            }
        }, data);

        return ret;
    };

    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto AssetProcessor::load_accessor_data_from_file(const std::filesystem::path& root_path, const fastgltf::Asset& gltf_asset, const fastgltf::Accessor& accessor) -> std::vector<ElemT> {
        PROFILE_SCOPE;

        static_assert(!IS_INDEX_BUFFER || std::is_same_v<ElemT, u32>, "Index Buffer must be u32");
        const fastgltf::BufferView& gltf_buffer_view = gltf_asset.bufferViews[accessor.bufferViewIndex.value()];
        const fastgltf::Buffer& gltf_buffer = gltf_asset.buffers[gltf_buffer_view.bufferIndex];
        
        const usize element_byte_size = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
        const usize total_bytes = accessor.count * element_byte_size;

        BufferData buffer_data = fastgltf_read_data(root_path, gltf_asset, gltf_buffer.data, total_bytes, gltf_buffer_view.byteOffset + accessor.byteOffset, accessor.byteOffset);
        
        std::vector<ElemT> ret(accessor.count);
        if constexpr (IS_INDEX_BUFFER) {
            if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                const u16* src = r_cast<const u16*>(buffer_data.data_span.data());
                for (size_t i = 0; i < accessor.count; ++i) {
                    ret[i] = s_cast<u32>(src[i]);
                }
            } else if (accessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                std::memcpy(ret.data(), buffer_data.data_span.data(), total_bytes);
            } else {
                throw std::runtime_error("Unsupported index component type");
            }
        } else {
            std::memcpy(ret.data(), buffer_data.data_span.data(), total_bytes);
        }
        
        return ret;
    }

    AssetProcessor::AssetProcessor(GPUContext* _gpu_context) : gpu_context{_gpu_context} {}
    AssetProcessor::~AssetProcessor() {}

    void AssetProcessor::process_mesh_load(const LoadMeshInfo& info) {
        PROFILE_SCOPE;

        const fastgltf::Mesh& gltf_mesh = info.asset->meshes[info.asset_local_mesh_group_index];
        const fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[info.asset_local_mesh_index];
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
            loaded_mesh.positions = load_accessor_data_from_file<glm::vec3, false>(info.asset_path.parent_path(), *info.asset, info.asset->accessors[position_attribute_iter->accessorIndex]);
        }

        u32 vertex_count = s_cast<u32>(loaded_mesh.positions.size());

        const fastgltf::Attribute* normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
        if(normal_attribute_iter != gltf_primitive.attributes.end()) {
            loaded_mesh.normals = load_accessor_data_from_file<glm::vec3, false>(info.asset_path.parent_path(), *info.asset, info.asset->accessors[normal_attribute_iter->accessorIndex]);
        } else {
            fill(loaded_mesh.normals, vertex_count);
        }

        const fastgltf::Attribute* uv_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
        if(uv_attribute_iter != gltf_primitive.attributes.end()) {
            loaded_mesh.uvs = load_accessor_data_from_file<glm::vec2, false>(info.asset_path.parent_path(), *info.asset, info.asset->accessors[uv_attribute_iter->accessorIndex]);
        } else {
            fill(loaded_mesh.uvs, vertex_count);
        }

        if(gltf_primitive.indicesAccessor.has_value()) {
            loaded_mesh.indices = load_accessor_data_from_file<u32, true>(info.asset_path.parent_path(), *info.asset, info.asset->accessors[gltf_primitive.indicesAccessor.value()]);
        }

        loaded_mesh = MeshProcessor::process_mesh(loaded_mesh);

        auto& positions = loaded_mesh.positions;
        auto& normals = loaded_mesh.normals;
        auto& uvs = loaded_mesh.uvs;
        auto& indices = loaded_mesh.indices;
        
        MeshGeometryData mesh_geometry_data = {};
        mesh_geometry_data.vertex_count = s_cast<u32>(positions.size());
        mesh_geometry_data.index_count = s_cast<u32>(indices.size());
        
        usize total_mesh_size = positions.size() * sizeof(glm::vec3) + normals.size() * sizeof(glm::vec3) + uvs.size() * sizeof(glm::vec2);

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

        LoadedImage loaded_image = {};
        const fastgltf::Image& gltf_image = info.asset->images[info.texture_manifest_index];
        if(const fastgltf::sources::URI* uri = std::get_if<fastgltf::sources::URI>(&gltf_image.data)) {
            loaded_image = ImageProcessor::load_image(info.asset_path.parent_path() / uri->uri.fspath());
        } else {
            BufferData data = fastgltf_read_data(info.asset_path, *info.asset, gltf_image.data);
            loaded_image = ImageProcessor::load_image(data.data_span);
        }

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
                    .name = fmt::format("{} blas", gpu_context->device.buffer_info(mesh_upload_info.mesh_buffer).value().name.c_str()).substr(0, 57)
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
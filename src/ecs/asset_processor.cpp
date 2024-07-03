#include "asset_processor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Shaper {
    AssetProcessor::AssetProcessor(Context* _context) : context{_context} {}
    AssetProcessor::~AssetProcessor() {}

    void AssetProcessor::load_mesh(const LoadMeshInfo& info) {
        fastgltf::Primitive primitive = info.asset->meshes[info.gltf_mesh_index].primitives[info.gltf_primitive_index];

        u32 count = 0;
        const glm::vec3* positions = nullptr;
        const glm::vec3* normals = nullptr;
        const glm::vec2* uvs = nullptr;

        if (primitive.findAttribute("POSITION") != primitive.attributes.end()) {
            auto& accessor = info.asset->accessors[primitive.findAttribute("POSITION")->second];
            auto& view = info.asset->bufferViews[accessor.bufferViewIndex.value()];
            positions = reinterpret_cast<const glm::vec3*>(&(std::get<fastgltf::sources::Vector>(info.asset->buffers[view.bufferIndex].data).bytes[accessor.byteOffset + view.byteOffset]));
            count = static_cast<u32>(accessor.count);
        }

        if (primitive.findAttribute("NORMAL") != primitive.attributes.end()) {
            auto& accessor = info.asset->accessors[primitive.findAttribute("NORMAL")->second];
            auto& view = info.asset->bufferViews[accessor.bufferViewIndex.value()];
            normals = reinterpret_cast<const glm::vec3*>(&(std::get<fastgltf::sources::Vector>(info.asset->buffers[view.bufferIndex].data).bytes[accessor.byteOffset + view.byteOffset]));
        }

        if (primitive.findAttribute("TEXCOORD_0") != primitive.attributes.end()) {
            auto& accessor = info.asset->accessors[primitive.findAttribute("TEXCOORD_0")->second];
            auto& view = info.asset->bufferViews[accessor.bufferViewIndex.value()];
            uvs = reinterpret_cast<const glm::vec2*>(&(std::get<fastgltf::sources::Vector>(info.asset->buffers[view.bufferIndex].data).bytes[accessor.byteOffset + view.byteOffset]));
        }

        std::vector<Vertex> vertices = {};
        vertices.resize(count);
        for(u32 i = 0; i < count; i++) {
            vertices[i] = Vertex {
                .position = *reinterpret_cast<const daxa_f32vec3*>(&positions[i]),
                // .normal = *reinterpret_cast<const daxa_f32vec3*>(&normals[i]),
                .uv = uvs ? *reinterpret_cast<const daxa_f32vec2*>(&uvs[i]) : daxa_f32vec2{0.0f, 0.0f},
            };
        }

        std::vector<u32> indices = {};
        u32 index_count = 0;
        {
            auto& accessor = info.asset->accessors[primitive.indicesAccessor.value()];
            auto& bufferView = info.asset->bufferViews[accessor.bufferViewIndex.value()];
            auto& buffer = info.asset->buffers[bufferView.bufferIndex];

            index_count = static_cast<u32>(accessor.count);

            switch(accessor.componentType) {
                case fastgltf::ComponentType::UnsignedInt: {
                    const uint32_t * buf = reinterpret_cast<const uint32_t *>(&std::get<fastgltf::sources::Vector>(buffer.data).bytes[accessor.byteOffset + bufferView.byteOffset]);
                    indices.reserve((indices.size() + accessor.count) * sizeof(uint32_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indices.push_back(buf[index]);
                    }
                    break;
                }
                case fastgltf::ComponentType::UnsignedShort: {
                    const uint16_t * buf = reinterpret_cast<const uint16_t *>(&std::get<fastgltf::sources::Vector>(buffer.data).bytes[accessor.byteOffset + bufferView.byteOffset]);
                    indices.reserve((indices.size() + accessor.count) * sizeof(uint16_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indices.push_back(buf[index]);
                    }
                    break;
                }
                case fastgltf::ComponentType::UnsignedByte: {
                    const uint8_t * buf = reinterpret_cast<const uint8_t *>(&std::get<fastgltf::sources::Vector>(buffer.data).bytes[accessor.byteOffset + bufferView.byteOffset]);
                    indices.reserve((indices.size() + accessor.count) * sizeof(uint8_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indices.push_back(buf[index]);
                    }
                    break;
                }
                default: { throw std::runtime_error("unhandled index buffer type"); }
            }
        }

        daxa::BufferId staging_buffer = context->device.create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(vertices.size() * sizeof(Vertex) + indices.size() * sizeof(u32)),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "staging buffer"
        });

        std::byte* ptr = context->device.get_host_address(staging_buffer).value();
        std::memcpy(ptr, vertices.data(), vertices.size() * sizeof(Vertex));
        ptr += vertices.size() * sizeof(Vertex);
        std::memcpy(ptr, indices.data(), indices.size() * sizeof(u32));

        daxa::BufferId vertex_buffer = context->device.create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(vertices.size() * sizeof(Vertex)),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "vertex buffer"
        });

        daxa::BufferId index_buffer = context->device.create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(indices.size() * sizeof(u32)),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "index buffer"
        });

        {
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            mesh_upload_queue.push_back(MeshUploadInfo {
                .staging_buffer = staging_buffer,
                .vertex_buffer = vertex_buffer,
                .index_buffer = index_buffer,
                .vertex_count = static_cast<u32>(vertices.size()),
                .index_count = static_cast<u32>(indices.size()),
                .manifest_index = info.manifest_index,
                .material_manifest_offset = info.material_manifest_offset
            });
        }
    }

    void AssetProcessor::load_texture(const LoadTextureInfo& info) {
        fastgltf::Asset const & gltf_asset = *info.asset;
        fastgltf::Image const & image = gltf_asset.images.at(info.gltf_texture_index);
        std::vector<std::byte> raw_data = {};
        i32 width = 0;
        i32 height = 0;
        i32 num_channels = 0;
        std::string image_path = info.asset_path.parent_path().string();

        auto get_data = [&](const fastgltf::Buffer& buffer, std::size_t byteOffset, std::size_t byteLength) {
            return std::visit(fastgltf::visitor {
                [](auto&) -> std::span<const std::byte> {
                    assert(false && "Tried accessing a buffer with no data, likely because no buffers were loaded. Perhaps you forgot to specify the LoadExternalBuffers option?");
                    return {};
                },
                [](const fastgltf::sources::Fallback& fallback) -> std::span<const std::byte> {
                    assert(false && "Tried accessing data of a fallback buffer.");
                    return {};
                },
                [&](const fastgltf::sources::Vector& vec) -> std::span<const std::byte> {
                    return std::span(reinterpret_cast<const std::byte*>(vec.bytes.data()), vec.bytes.size());
                },
                [&](const fastgltf::sources::ByteView& bv) -> std::span<const std::byte> {
                    return bv.bytes;
                },
            }, buffer.data).subspan(byteOffset, byteLength);
        };

        {
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            if(const auto* data = std::get_if<std::monostate>(&image.data)) {
                std::cout << "std::monostate" << std::endl;
            }
            if(const auto* data = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
                auto& buffer_view = gltf_asset.bufferViews[data->bufferViewIndex];
                auto content = get_data(gltf_asset.buffers[buffer_view.bufferIndex], buffer_view.byteOffset, buffer_view.byteLength);
                u8* image_data = stbi_load_from_memory(r_cast<const u8*>(content.data()), s_cast<i32>(content.size_bytes()), &width, &height, &num_channels, 4);
                if(!image_data) { std::cout << "bozo" << std::endl; }
                raw_data.resize(width * height * 4);
                std::memcpy(raw_data.data(), image_data, width * height * 4);
                stbi_image_free(image_data);
            }
            if(const auto* data = std::get_if<fastgltf::sources::URI>(&image.data)) {
                image_path = info.asset_path.parent_path().string() + '/' + std::string(data->uri.path().begin(), data->uri.path().end());
                u8* image_data = stbi_load(image_path.c_str(), &width, &height, &num_channels, 4);
                if(!image_data) { std::cout << "bozo" << std::endl; }
                raw_data.resize(width * height * 4);
                std::memcpy(raw_data.data(), image_data, width * height * 4);
                stbi_image_free(image_data);
            }
            if(const auto* data = std::get_if<fastgltf::sources::Vector>(&image.data)) {
                std::cout << "fastgltf::sources::Vector" << std::endl;
            }
            if(const auto* data = std::get_if<fastgltf::sources::CustomBuffer>(&image.data)) {
                std::cout << "fastgltf::sources::CustomBuffer" << std::endl;
            }
            if(const auto* data = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
                std::cout << "fastgltf::sources::ByteView" << std::endl;
            }
        }

        u32 mip_levels = static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;
        daxa::Format daxa_format = info.load_as_srgb ? daxa::Format::R8G8B8A8_SRGB : daxa::Format::R8G8B8A8_UNORM;
        daxa::ImageId daxa_image = context->device.create_image(daxa::ImageInfo {
            .dimensions = 2,
            .format = daxa_format,
            .size = {static_cast<u32>(width), static_cast<u32>(height), 1},
            .mip_level_count = mip_levels,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::TRANSFER_SRC,
            .name = image_path,
        });

        daxa::SamplerId daxa_sampler = context->device.create_sampler({
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
            .max_lod = static_cast<f32>(mip_levels),
            .enable_unnormalized_coordinates = false,
        });

        daxa::BufferId staging_buffer = context->device.create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(width * height * 4),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "staging buffer"
        }); 

        std::memcpy(context->device.get_host_address(staging_buffer).value(), raw_data.data(), width * height * 4);

        {
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            texture_upload_queue.push_back(TextureUploadInfo{
                .staging_buffer = staging_buffer,
                .dst_image = daxa_image,
                .sampler = daxa_sampler,
                .manifest_index = info.texture_manifest_index,
            });
        }
    }

    auto AssetProcessor::record_gpu_load_processing_commands() -> RecordCommands {
        RecordCommands ret = {};
        {
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            ret.uploaded_meshes = std::move(mesh_upload_queue);
            mesh_upload_queue = {};
        }

        auto cmd_recorder = context->device.create_command_recorder(daxa::CommandRecorderInfo { .name = "asset processor upload" });

        for(auto& mesh_upload_info : ret.uploaded_meshes) {
            cmd_recorder.destroy_buffer_deferred(mesh_upload_info.staging_buffer);

            u32 vertex_buffer_size = context->device.info_buffer(mesh_upload_info.vertex_buffer).value().size;
            u32 index_buffer_size = context->device.info_buffer(mesh_upload_info.index_buffer).value().size;

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = mesh_upload_info.staging_buffer,
                .dst_buffer = mesh_upload_info.vertex_buffer,
                .src_offset = 0,
                .dst_offset = 0,
                .size = vertex_buffer_size
            });

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = mesh_upload_info.staging_buffer,
                .dst_buffer = mesh_upload_info.index_buffer,
                .src_offset = vertex_buffer_size,
                .dst_offset = 0,
                .size = index_buffer_size
            });
        }

        {
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            ret.uploaded_textures = std::move(texture_upload_queue);
            texture_upload_queue = {};
        }

        for(auto& texture_upload_info : ret.uploaded_textures) {
            cmd_recorder.destroy_buffer_deferred(texture_upload_info.staging_buffer);

            // cmd_recorder.pipeline_barrier_image_transition({
            //     .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
            //     .dst_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            //     .image_id = texture_upload_info.dst_image,
            // });

            // cmd_recorder.copy_buffer_to_image({
            //     .buffer = texture_upload_info.staging_buffer,
            //     .image = texture_upload_info.dst_image,
            //     .image_offset = {0, 0, 0},
            //     .image_extent = context->device.info_image(texture_upload_info.dst_image).value().size,
            // });
            
            // cmd_recorder.pipeline_barrier_image_transition({
            //     .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            //     .dst_access = daxa::AccessConsts::TOP_OF_PIPE_READ_WRITE,
            //     .src_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            //     .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            //     .image_id = texture_upload_info.dst_image,
            // });

            auto image_info = context->device.info_image(texture_upload_info.dst_image).value();

            cmd_recorder.pipeline_barrier_image_transition({
                .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                .dst_access = daxa::AccessConsts::READ_WRITE,
                .src_layout = daxa::ImageLayout::UNDEFINED,
                .dst_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = image_info.mip_level_count,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = texture_upload_info.dst_image,
            });

            cmd_recorder.copy_buffer_to_image({
                .buffer = texture_upload_info.staging_buffer,
                .buffer_offset = 0,
                .image = texture_upload_info.dst_image,
                .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .image_slice = {
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_offset = { 0, 0, 0 },
                .image_extent = { image_info.size.x, image_info.size.y, 1 }
            });

            cmd_recorder.pipeline_barrier({
                .src_access = daxa::AccessConsts::HOST_WRITE,
                .dst_access = daxa::AccessConsts::TRANSFER_READ,
            });

            std::array<i32, 3> mip_size = {
                static_cast<i32>(image_info.size.x),
                static_cast<i32>(image_info.size.y),
                static_cast<i32>(image_info.size.z),
            };

            for(u32 i = 1; i < image_info.mip_level_count; i++) {
                cmd_recorder.pipeline_barrier_image_transition({
                    .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .dst_access = daxa::AccessConsts::BLIT_READ,
                    .src_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                    .dst_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                    .image_slice = {
                        .base_mip_level = i - 1,
                        .level_count = 1,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .image_id = texture_upload_info.dst_image,
                });

                std::array<i32, 3> next_mip_size = {
                    std::max<i32>(1, mip_size[0] / 2),
                    std::max<i32>(1, mip_size[1] / 2),
                    std::max<i32>(1, mip_size[2] / 2),
                };

                cmd_recorder.blit_image_to_image({
                    .src_image = texture_upload_info.dst_image,
                    .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                    .dst_image = texture_upload_info.dst_image,
                    .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                    .src_slice = {
                        .mip_level = i - 1,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .src_offsets = {{{0, 0, 0}, {mip_size[0], mip_size[1], mip_size[2]}}},
                    .dst_slice = {
                        .mip_level = i,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .dst_offsets = {{{0, 0, 0}, {next_mip_size[0], next_mip_size[1], next_mip_size[2]}}},
                    .filter = daxa::Filter::LINEAR,
                });

                cmd_recorder.pipeline_barrier_image_transition({
                    .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .dst_access = daxa::AccessConsts::BLIT_READ,
                    .src_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                    .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                    .image_slice = {
                        .base_mip_level = i - 1,
                        .level_count = 1,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .image_id = texture_upload_info.dst_image,
                });
                
                mip_size = next_mip_size;
            }

            cmd_recorder.pipeline_barrier_image_transition({
                .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                .dst_access = daxa::AccessConsts::READ_WRITE,
                .src_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .base_mip_level = image_info.mip_level_count - 1,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = texture_upload_info.dst_image,
            });
        }

        ret.upload_commands = cmd_recorder.complete_current_commands();
        return ret;
    }
}
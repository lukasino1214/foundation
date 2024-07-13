#include "asset_processor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <meshoptimizer.h>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace Shaper {
    AssetProcessor::AssetProcessor(Context* _context) : context{_context}, mesh_upload_mutex{std::make_unique<std::mutex>()} {}
    AssetProcessor::~AssetProcessor() {}

    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto load_data(fastgltf::Asset& asset, fastgltf::Accessor& accessor, u32 required_size = 0) {
        auto& buffer_view = asset.bufferViews[accessor.bufferViewIndex.value()];
        
        std::vector<ElemT> ret(accessor.count);
        if constexpr(IS_INDEX_BUFFER) {
            if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                std::vector<u16> u16_index_buffer(accessor.count);
                fastgltf::copyFromAccessor<u16>(asset, accessor, u16_index_buffer.data());
                for (size_t i = 0; i < u16_index_buffer.size(); ++i) {
                    ret[i] = s_cast<u32>(u16_index_buffer[i]);
                }
            }
            else {
                fastgltf::copyFromAccessor<u32>(asset, accessor, ret.data());
            }
        } else {
            fastgltf::copyFromAccessor<ElemT>(asset, accessor, ret.data());
        }

        return ret;
    }

    void AssetProcessor::load_mesh(const LoadMeshInfo& info) {
        ZoneScoped;
        fastgltf::Asset& gltf_asset = *info.asset;
        fastgltf::Mesh& gltf_mesh = info.asset->meshes[info.gltf_mesh_index];
        fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[info.gltf_primitive_index];

        std::vector<glm::vec3> vert_positions = {};
        auto position_attribute_iter = gltf_primitive.findAttribute("POSITION");
        if(position_attribute_iter != gltf_primitive.attributes.end()) {
            vert_positions = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[position_attribute_iter->second], 0);
        }

        auto fill = [](auto& vec, u32 required_size = 0) {
            if(vec.empty()) {
                vec.resize(required_size);
                for(u32 i = 0; i < required_size; i++) {
                    vec[i] = {};
                }
            }
        };

        u32 vertex_count = vert_positions.size();
        std::vector<glm::vec3> vert_normals = {};
        auto normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
        if(normal_attribute_iter != gltf_primitive.attributes.end()) {
            vert_normals = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[normal_attribute_iter->second], vertex_count);
        } else {
            fill(vert_normals, vertex_count);
        }

        std::vector<glm::vec2> vert_uvs = {};
        auto uvs_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
        if(uvs_attribute_iter != gltf_primitive.attributes.end()) {
            vert_uvs = load_data<glm::vec2, false>(gltf_asset, gltf_asset.accessors[uvs_attribute_iter->second], vertex_count);
        } else {
            fill(vert_uvs, vertex_count);
        }

        std::vector<u32> indices = {};
        indices = load_data<u32, true>(gltf_asset, gltf_asset.accessors[gltf_primitive.indicesAccessor.value()]);

        std::vector<Vertex> vertices = {};
        vertices.resize(vertex_count);
        for(u32 i = 0; i < vertex_count; i++) {
            vertices[i] = Vertex {
                .position = *r_cast<daxa_f32vec3*>(&vert_positions[i]),
                .normal = *r_cast<daxa_f32vec3*>(&vert_normals[i]),
                .uv = *r_cast<daxa_f32vec2*>(&vert_uvs[i]),
            };
        }

        constexpr usize MAX_VERTICES = MAX_VERTICES_PER_MESHLET;
        constexpr usize MAX_TRIANGLES = MAX_TRIANGLES_PER_MESHLET;
        constexpr float CONE_WEIGHT = 1.0f;

        {
            std::vector<u32> optimized_indices(indices.size());
            meshopt_optimizeVertexCache(optimized_indices.data(), indices.data(), indices.size(), vertices.size());
            indices = std::move(optimized_indices);
        }

        daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(vertices.size() * sizeof(Vertex) + indices.size() * sizeof(u32)),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "staging buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.gltf_mesh_index) + " primitive " + std::to_string(info.gltf_primitive_index)
        });

        std::byte* ptr = context->device.get_host_address(staging_buffer).value();
        std::memcpy(ptr, vertices.data(), vertices.size() * sizeof(Vertex));
        ptr += vertices.size() * sizeof(Vertex);
        std::memcpy(ptr, indices.data(), indices.size() * sizeof(u32));

        daxa::BufferId vertex_buffer = context->create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(vertices.size() * sizeof(Vertex)),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "vertex buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.gltf_mesh_index) + " primitive " + std::to_string(info.gltf_primitive_index)
        });

        daxa::BufferId index_buffer = context->create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(indices.size() * sizeof(u32)),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "index buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.gltf_mesh_index) + " primitive " + std::to_string(info.gltf_primitive_index)
        });

        size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), MAX_VERTICES, MAX_TRIANGLES);
        std::vector<meshopt_Meshlet> meshlets(max_meshlets);
        std::vector<u32> meshlet_indirect_vertices(max_meshlets * MAX_VERTICES);
        std::vector<u8> meshlet_micro_indices(max_meshlets * MAX_TRIANGLES * 3);

        size_t meshlet_count = meshopt_buildMeshlets(
            meshlets.data(),
            meshlet_indirect_vertices.data(),
            meshlet_micro_indices.data(),
            indices.data(),
            indices.size(),
            r_cast<const f32*>(vert_positions.data()),
            s_cast<usize>(vertices.size()),
            sizeof(glm::vec3),
            MAX_VERTICES,
            MAX_TRIANGLES,
            CONE_WEIGHT
        );

        std::vector<BoundingSphere> meshlet_bounds(meshlet_count);
        std::vector<AABB> meshlet_aabbs(meshlet_count);
        for (size_t meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
            meshopt_Bounds raw_bounds = meshopt_computeMeshletBounds(
                &meshlet_indirect_vertices[meshlets[meshlet_index].vertex_offset],
                &meshlet_micro_indices[meshlets[meshlet_index].triangle_offset],
                meshlets[meshlet_index].triangle_count,
                r_cast<const f32*>(vert_positions.data()),
                s_cast<usize>(vertices.size()),
                sizeof(glm::vec3));
            meshlet_bounds[meshlet_index].center.x = raw_bounds.center[0];
            meshlet_bounds[meshlet_index].center.y = raw_bounds.center[1];
            meshlet_bounds[meshlet_index].center.z = raw_bounds.center[2];
            meshlet_bounds[meshlet_index].radius = raw_bounds.radius;

            glm::vec3 min_pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].vertex_offset]];
            glm::vec3 max_pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].vertex_offset]];

            for (u32 vertex_index = 0; vertex_index < meshlets[meshlet_index].vertex_count; ++vertex_index) {
                glm::vec3 pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].vertex_offset + vertex_index]];
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }

            glm::vec3 center = (max_pos + min_pos) * 0.5f;
            glm::vec3 extent = max_pos - center;

            meshlet_aabbs[meshlet_index].center = std::bit_cast<daxa_f32vec3>(center);
            meshlet_aabbs[meshlet_index].extent = std::bit_cast<daxa_f32vec3>(extent);
        }

        const meshopt_Meshlet& last = meshlets[meshlet_count - 1];
        meshlet_indirect_vertices.resize(last.vertex_offset + last.vertex_count);
        meshlet_micro_indices.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
        meshlets.resize(meshlet_count);

        const u64 total_mesh_buffer_size =
            sizeof(Meshlet) * meshlet_count +
            sizeof(BoundingSphere) * meshlet_count +
            sizeof(AABB) * meshlet_count +
            sizeof(u8) * meshlet_micro_indices.size() +
            sizeof(u32) * meshlet_indirect_vertices.size() +
            sizeof(daxa_f32vec3) * vertices.size() +
            sizeof(daxa_f32vec2) * vertices.size() +
            sizeof(daxa_f32vec3) * vertices.size();

        Mesh mesh = {};

        daxa::DeviceAddress mesh_bda = {};
        daxa::BufferId staging_mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};

        {
            mesh_buffer = context->create_buffer(daxa::BufferInfo {
                .size = s_cast<daxa::usize>(total_mesh_buffer_size),
                .name = "mesh buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.gltf_mesh_index) + " primitive " + std::to_string(info.gltf_primitive_index)
            });

            mesh_bda = context->device.get_device_address(std::bit_cast<daxa::BufferId>(mesh_buffer)).value();

            staging_mesh_buffer = context->create_buffer(daxa::BufferInfo {
                .size = s_cast<daxa::usize>(total_mesh_buffer_size),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.gltf_mesh_index) + " primitive " + std::to_string(info.gltf_primitive_index) + " staging",
            });
        }

        std::byte* staging_ptr = context->device.get_host_address(staging_mesh_buffer).value();
        u32 accumulated_offset = 0;

        mesh.meshlets = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, meshlets.data(), meshlets.size() * sizeof(Meshlet));
        accumulated_offset += sizeof(Meshlet) * meshlet_count;
        
        mesh.meshlet_bounds = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, meshlet_bounds.data(), meshlet_bounds.size() * sizeof(BoundingSphere));
        accumulated_offset += sizeof(BoundingSphere) * meshlet_count;
        
        mesh.meshlet_aabbs = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, meshlet_aabbs.data(), meshlet_aabbs.size() * sizeof(AABB));
        accumulated_offset += sizeof(AABB) * meshlet_count;
        
        mesh.micro_indices = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, meshlet_micro_indices.data(), meshlet_micro_indices.size() * sizeof(u8));
        accumulated_offset += sizeof(u8) * meshlet_micro_indices.size();
        
        mesh.indirect_vertices = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, meshlet_indirect_vertices.data(), meshlet_indirect_vertices.size() * sizeof(u32));
        accumulated_offset += sizeof(u32) * meshlet_indirect_vertices.size();
        
        mesh.vertex_positions = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, vert_positions.data(), vertices.size() * sizeof(daxa_f32vec3));
        accumulated_offset += sizeof(daxa_f32vec3) * vertices.size();
        
        mesh.vertex_normals = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, vert_normals.data(), vertices.size() * sizeof(daxa_f32vec3));
        accumulated_offset += sizeof(daxa_f32vec3) * vertices.size();

        mesh.vertex_uvs = mesh_bda + accumulated_offset;
        std::memcpy(staging_ptr + accumulated_offset, vert_uvs.data(), vertices.size() * sizeof(daxa_f32vec2));
        accumulated_offset += sizeof(daxa_f32vec2) * vertices.size();
        
        
        mesh.material_index = info.material_manifest_offset + static_cast<u32>(info.asset->meshes[info.gltf_mesh_index].primitives[info.gltf_primitive_index].materialIndex.value());
        mesh.meshlet_count = s_cast<u32>(meshlet_count);
        mesh.vertex_count = s_cast<u32>(vertices.size());

        {
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            mesh_upload_queue.push_back(MeshUploadInfo {
                .staging_buffer = staging_buffer,
                .vertex_buffer = vertex_buffer,
                .index_buffer = index_buffer,
                .vertex_count = static_cast<u32>(vertices.size()),
                .index_count = static_cast<u32>(indices.size()),
                .staging_mesh_buffer = staging_mesh_buffer,
                .mesh_buffer = mesh_buffer,
                .mesh = mesh,
                .manifest_index = info.manifest_index,
                .material_manifest_offset = info.material_manifest_offset
            });
        }
    }

    void AssetProcessor::load_texture(const LoadTextureInfo& info) {
        ZoneScoped;
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
        daxa::ImageId daxa_image = context->create_image(daxa::ImageInfo {
            .dimensions = 2,
            .format = daxa_format,
            .size = {static_cast<u32>(width), static_cast<u32>(height), 1},
            .mip_level_count = mip_levels,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::TRANSFER_SRC,
            .name = "image: " + image_path + " " + std::to_string(info.gltf_texture_index),
        });

        daxa::SamplerId daxa_sampler = context->get_sampler({
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

        daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
            .size = static_cast<u32>(width * height * 4),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "staging buffer: " + image_path + " " + std::to_string(info.gltf_texture_index)
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
        ZoneScoped;
        RecordCommands ret = {};
        {
            ZoneNamedN(getting_meshes, "getting_meshes", true);
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            ret.uploaded_meshes = std::move(mesh_upload_queue);
            mesh_upload_queue = {};
        }

        auto cmd_recorder = context->device.create_command_recorder(daxa::CommandRecorderInfo { .name = "asset processor upload" });

        {
            ZoneNamedN(mesh_upload_info, "mesh_upload_info", true);
            for(auto& mesh_upload_info : ret.uploaded_meshes) {
                context->destroy_buffer_deferred(cmd_recorder, mesh_upload_info.staging_buffer);
                context->destroy_buffer_deferred(cmd_recorder, mesh_upload_info.staging_mesh_buffer);

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

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = mesh_upload_info.staging_mesh_buffer,
                    .dst_buffer = mesh_upload_info.mesh_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = context->device.info_buffer(mesh_upload_info.mesh_buffer).value().size
                });
            }
        }

        {
            ZoneNamedN(getting_textures, "getting_textures", true);
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            ret.uploaded_textures = std::move(texture_upload_queue);
            texture_upload_queue = {};
        }

        {
            ZoneNamedN(texture_upload_info, "texture_upload_info", true);
            for(auto& texture_upload_info : ret.uploaded_textures) {
                context->destroy_buffer_deferred(cmd_recorder, texture_upload_info.staging_buffer);

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
        }

        ret.upload_commands = cmd_recorder.complete_current_commands();
        return ret;
    }
}
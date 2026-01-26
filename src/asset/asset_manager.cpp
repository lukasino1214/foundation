#include "asset_manager.hpp"
#include "graphics/gpu_context.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmaybe-uninitialized"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#pragma clang diagnostic pop

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ecs/entity.hpp>
#include <ecs/scene.hpp>

#include <math/math.hpp>
#include "asset_processor.hpp"

namespace foundation {
    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto load_accessor_data_from_file(
        std::filesystem::path const & root_path,
        fastgltf::Asset const & gltf_asset,
        fastgltf::Accessor const & accesor)
        -> std::vector<ElemT>
    {
        static_assert(!IS_INDEX_BUFFER || std::is_same_v<ElemT, u32>, "Index Buffer must be u32");
        fastgltf::BufferView const & gltf_buffer_view = gltf_asset.bufferViews.at(accesor.bufferViewIndex.value());
        fastgltf::Buffer const & gltf_buffer = gltf_asset.buffers.at(gltf_buffer_view.bufferIndex);
        if (!std::holds_alternative<fastgltf::sources::URI>(gltf_buffer.data))
        {
        }
        fastgltf::sources::URI uri = std::get<fastgltf::sources::URI>(gltf_buffer.data);

        std::filesystem::path const full_buffer_path = root_path / uri.uri.fspath();
        std::ifstream ifs{full_buffer_path, std::ios::binary};
        if (!ifs)
        {
        }
        ifs.seekg(s_cast<std::streamsize>(gltf_buffer_view.byteOffset + accesor.byteOffset + uri.fileByteOffset));
        std::vector<u16> raw = {};
        size_t const elem_byte_size = fastgltf::getElementByteSize(accesor.type, accesor.componentType);
        raw.resize((accesor.count * elem_byte_size) / 2);
        if (!ifs.read(r_cast<char *>(raw.data()), s_cast<std::streamsize>(accesor.count * elem_byte_size)))
        {
        }
        auto buffer_adapter = [&](fastgltf::Asset const & /* asset */, u32 /* asset_index */) -> fastgltf::span<std::byte const>
        {
            return fastgltf::span<std::byte const>(reinterpret_cast<std::byte const *>(raw.data()) - accesor.byteOffset, accesor.count * elem_byte_size);
        };

        std::vector<ElemT> ret(accesor.count);
        if constexpr (IS_INDEX_BUFFER)
        {
            if (accesor.componentType == fastgltf::ComponentType::UnsignedShort)
            {
                std::vector<u16> u16_index_buffer(accesor.count);
                fastgltf::copyFromAccessor<u16>(gltf_asset, accesor, u16_index_buffer.data(), buffer_adapter);
                for (size_t i = 0; i < u16_index_buffer.size(); ++i)
                {
                    ret[i] = s_cast<u32>(u16_index_buffer[i]);
                }
            }
            else
            {
                fastgltf::copyFromAccessor<u32>(gltf_asset, accesor, ret.data(), buffer_adapter);
            }
        }
        else
        {
            fastgltf::copyFromAccessor<ElemT>(gltf_asset, accesor, ret.data(), buffer_adapter);
        }
        return ret;
    }

    auto AssetManager::load(const std::filesystem::path& file_path, GPUContext* gpu_context, Scene* scene, Entity root_entity) -> ModelData {
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error(fmt::format("File does not exist with path: {}", file_path.string()));
        }

        std::unique_ptr<fastgltf::Asset> asset = {};
        {
            fastgltf::Parser parser{ fastgltf::Extensions::KHR_lights_punctual };
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble;

            auto gltf_file = fastgltf::MappedGltfFile::FromPath(file_path);
            
            if(gltf_file.error() != fastgltf::Error::None) {
                throw std::runtime_error(fmt::format("loading gltf file failed reason: {}", fastgltf::getErrorName(gltf_file.error())));
            }
            
            auto fastgltf_asset = parser.loadGltf(gltf_file.get(), file_path.parent_path(), gltf_options);
            
            if(fastgltf_asset.error() != fastgltf::Error::None) {
                throw std::runtime_error(fmt::format("parsing gltf file failed reason: {}", fastgltf::getErrorName(fastgltf_asset.error())));
            }
            
            asset = std::make_unique<fastgltf::Asset>(std::move(fastgltf_asset.get()));
        }

        fmt::println("loading asset: {}", file_path.string());
        fmt::println("scene count: {}", asset->scenes.size());
        fmt::println("node count: {}", asset->nodes.size());
        fmt::println("mesh count: {}", asset->meshes.size());
        fmt::println("material count: {}", asset->materials.size());
        fmt::println("image count: {}", asset->images.size());
        fmt::println("texture count: {}", asset->textures.size());
        fmt::println("sampler count: {}", asset->samplers.size());

        ModelData model_data = {};
        model_data.file_path = file_path;

        model_data.scenes.reserve(asset->scenes.size());
        for (const fastgltf::Scene& gltf_scene : asset->scenes) {
            IScene scene_ = {};
            scene_.name = gltf_scene.name.c_str();

            scene_.node_indices.reserve(gltf_scene.nodeIndices.size());
            for (usize value : gltf_scene.nodeIndices) {
                scene_.node_indices.push_back(s_cast<u32>(value));
            }

            model_data.scenes.push_back(scene_);
        }

        std::vector<flecs::entity> node_index_to_entity = {};
        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            Entity e = scene->create_entity(std::format("asset {} {} {}", asset->nodes[node_index].name, root_entity.get_name().data(), node_index));
            node_index_to_entity.push_back(e.get_handle());
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            const auto& node = asset->nodes[node_index];
            Entity parent_entity(node_index_to_entity[node_index], scene);
            if(node.meshIndex.has_value()) {
                parent_entity.add_component<MeshComponent>();
                auto& mesh_component = parent_entity.get_component<MeshComponent>();
                mesh_component.mesh_group_manifest_entry_index = s_cast<u32>(node.meshIndex.value());
            }

            glm::vec3 position = {};
            glm::quat rotation = {};
            glm::vec3 scale = {};
            if(const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
                rotation.x = trs->rotation.x();
                rotation.y = trs->rotation.y();
                rotation.z = trs->rotation.z();
                rotation.w = trs->rotation.w();

                position = *r_cast<const glm::vec3*>(&trs->translation);
                scale = *r_cast<const glm::vec3*>(&trs->scale);
            } else if(const auto* transform_matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
                fastgltf::math::fvec3 fastgltf_position;
                fastgltf::math::fquat fastgltf_rotation;
                fastgltf::math::fvec3 fastgltf_scale;
                fastgltf::math::decomposeTransformMatrix(*transform_matrix, fastgltf_scale, fastgltf_rotation, fastgltf_position);

                rotation.x = fastgltf_rotation.x();
                rotation.y = fastgltf_rotation.y();
                rotation.z = fastgltf_rotation.z();
                rotation.w = fastgltf_rotation.w();

                position = *r_cast<glm::vec3*>(&fastgltf_position);
                scale = *r_cast<glm::vec3*>(&fastgltf_scale);
            }

            parent_entity.add_component<TransformComponent>();
            parent_entity.set_local_position(position);
            parent_entity.set_local_rotation(rotation);
            parent_entity.set_local_scale(scale);

            for(usize children_index : node.children) {
                Entity child_entity(node_index_to_entity[children_index], scene);
                parent_entity.set_child(child_entity);
            }
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            Entity entity(node_index_to_entity[node_index], scene);

            if(!entity.has_parent()) {
                root_entity.set_child(entity);
            }
        }

        daxa::CommandRecorder cmd_recorder = gpu_context->device.create_command_recorder({ .name = "upload" });

        model_data.mesh_groups.reserve(asset->meshes.size());
        for (u32 mesh_group_index = 0; mesh_group_index < asset->meshes.size(); mesh_group_index++) {
            const fastgltf::Mesh& gltf_mesh = asset->meshes[mesh_group_index];
            MeshGroup mesh_group = {};
            mesh_group.name = gltf_mesh.name.c_str();

            mesh_group.meshes.reserve(gltf_mesh.primitives.size());
            for (u32 mesh_index = 0; mesh_index < gltf_mesh.primitives.size(); mesh_index++) {
                const fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[mesh_index];

                Mesh mesh = {};
                mesh.material_index = gltf_primitive.materialIndex.has_value() ? std::make_optional(s_cast<u32>(gltf_primitive.materialIndex.value())) : std::nullopt;

                LoadedMesh loaded_mesh = AssetProcessor::load_mesh(asset.get(), file_path, mesh_group_index, mesh_index);
                auto& positions = loaded_mesh.positions;
                auto& normals = loaded_mesh.normals;
                auto& uvs = loaded_mesh.uvs;
                auto& indices = loaded_mesh.indices;

                usize mesh_buffer_size = (positions.size() * sizeof(glm::vec3)) + (normals.size() * sizeof(glm::vec3)) + (uvs.size() * sizeof(glm::vec2));

                daxa::BufferId staging_mesh_buffer = gpu_context->device.create_buffer({
                    .size = mesh_buffer_size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "staging_mesh_buffer",
                });

                mesh.mesh_buffer = gpu_context->device.create_buffer({
                    .size = mesh_buffer_size,
                    .name = "mesh_buffer",
                });

                std::byte* mapped_ptr = gpu_context->device.buffer_host_address(staging_mesh_buffer).value();
                std::memcpy(mapped_ptr, positions.data(), positions.size() * sizeof(glm::vec3));
                mapped_ptr += positions.size() * sizeof(glm::vec3);
                std::memcpy(mapped_ptr, normals.data(), normals.size() * sizeof(glm::vec3));
                mapped_ptr += normals.size() * sizeof(glm::vec3);
                std::memcpy(mapped_ptr, uvs.data(), uvs.size() * sizeof(glm::vec2));
                mapped_ptr += uvs.size() * sizeof(glm::vec2);

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_mesh_buffer,
                    .dst_buffer = mesh.mesh_buffer,
                    .size = mesh_buffer_size
                });
                cmd_recorder.destroy_buffer_deferred(staging_mesh_buffer);
                
                daxa::BufferId staging_index_buffer = gpu_context->device.create_buffer({
                    .size = sizeof(u32) * indices.size(),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "staging_vertex_buffer",
                });

                mesh.index_buffer = gpu_context->device.create_buffer({
                    .size = sizeof(u32) * indices.size(),
                    .name = "index_buffer",
                });

                std::memcpy(gpu_context->device.buffer_host_address(staging_index_buffer).value(), indices.data(), sizeof(u32) * indices.size());
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_index_buffer,
                    .dst_buffer = mesh.index_buffer,
                    .size = sizeof(u32) * indices.size()
                });
                cmd_recorder.destroy_buffer_deferred(staging_index_buffer);

                mesh.mesh.positions = gpu_context->device.buffer_device_address(mesh.mesh_buffer).value();
                mesh.mesh.normals = mesh.mesh.positions + (positions.size() * sizeof(glm::vec3));
                mesh.mesh.uvs = mesh.mesh.normals + (normals.size() * sizeof(glm::vec3));

                mesh.mesh.vertex_count = s_cast<u32>(positions.size());
                mesh.mesh.material_index = s_cast<u32>(gltf_primitive.materialIndex.value_or(INVALID_ID));

                mesh.vertex_count = s_cast<u32>(positions.size());
                mesh.index_count = s_cast<u32>(indices.size());

                mesh_group.meshes.push_back(mesh);
            }

            model_data.mesh_groups.push_back(mesh_group);
        }

        model_data.images.reserve(asset->images.size());
        for(const fastgltf::Image& gltf_image : asset->images) {
            Image image = {};
            image.name = gltf_image.name.c_str();

            const auto* uri = std::get_if<fastgltf::sources::URI>(&gltf_image.data);
            if(uri == nullptr) { throw std::runtime_error("huh"); }
            LoadedImage loaded_image = AssetProcessor::load_image(file_path.parent_path() / uri->uri.fspath());

            daxa::BufferId staging_image_buffer = gpu_context->device.create_buffer({
                .size = loaded_image.data.size(),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = fmt::format("staging_texture_{}", image.name),
            });
            cmd_recorder.destroy_buffer_deferred(staging_image_buffer);

            std::memcpy(gpu_context->device.buffer_host_address(staging_image_buffer).value(), loaded_image.data.data(), loaded_image.data.size());

            u32 mip_level_count = loaded_image.mipmaps.empty() ? s_cast<u32>(std::floor(std::log2(std::max(loaded_image.width, loaded_image.height)))) + 1 : s_cast<u32>(loaded_image.mipmaps.size());
            image.image = gpu_context->device.create_image({
                .format = loaded_image.format,
                .size = { loaded_image.width, loaded_image.height, loaded_image.depth },
                .mip_level_count = mip_level_count,
                .array_layer_count = 1,
                .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
                .name = image.name,
            });

            image.sampler = gpu_context->device.create_sampler(daxa::SamplerInfo{
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
            });

            cmd_recorder.pipeline_image_barrier(daxa::ImageBarrierInfo {
                .image_id = image.image,
                .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
            });
           
            if(loaded_image.mipmaps.empty()) {
                cmd_recorder.copy_buffer_to_image({
                    .buffer = staging_image_buffer,
                    .buffer_offset = 0,
                    .image = image.image,
                    .image_slice = {
                        .mip_level = 0,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .image_offset = { 0, 0, 0 },
                    .image_extent = { loaded_image.width, loaded_image.height, loaded_image.depth }
                });

                std::array<i32, 3> mip_size = {
                    s_cast<i32>(loaded_image.width),
                    s_cast<i32>(loaded_image.height),
                    s_cast<i32>(loaded_image.depth),
                };

                for(u32 i = 1; i < mip_level_count; i++) {
                    std::array<i32, 3> next_mip_size = {
                        std::max<i32>(1, mip_size[0] / 2),
                        std::max<i32>(1, mip_size[1] / 2),
                        std::max<i32>(1, mip_size[2] / 2),
                    };

                    cmd_recorder.blit_image_to_image({
                        .src_image = image.image,
                        .dst_image = image.image,
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
                    
                    mip_size = next_mip_size;
                }
            } else {
                usize staging_offset = 0;
                for(u32 mip_level = 0; mip_level < loaded_image.mipmaps.size(); ++mip_level) {
                    const auto& mipmap = loaded_image.mipmaps[mip_level];
                    
                    cmd_recorder.copy_buffer_to_image({
                        .buffer = staging_image_buffer,
                        .buffer_offset = staging_offset,
                        .image = image.image,
                        .image_slice = {
                            .mip_level = mip_level,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_offset = {0, 0, 0},
                        .image_extent = {
                            std::max(1u, loaded_image.width >> mip_level),
                            std::max(1u, loaded_image.height >> mip_level),
                            std::max(1u, loaded_image.depth >> mip_level),
                        },
                    });

                    staging_offset += mipmap.size();
                }
            }

            model_data.images.push_back(image);
        }

        model_data.materials_buffer = gpu_context->device.create_buffer({
            .size = sizeof(GPUMaterial) * asset->materials.size(),
            .name = "materials_buffer",
        });

        // model_data.materials.reserve(asset->materials.size());
        
        std::vector<GPUMaterial> materials;
        materials.reserve(asset->materials.size());
        for(const fastgltf::Material& gltf_material : asset->materials) {
            GPUMaterial gpu_material = {};
            if(gltf_material.pbrData.baseColorTexture.has_value()) {
                const fastgltf::Texture& gltf_texture = asset->textures[gltf_material.pbrData.baseColorTexture.value().textureIndex];
                gpu_material.albedo_image_id = model_data.images[gltf_texture.imageIndex.value()].image.default_view();
                gpu_material.albedo_sampler_id = model_data.images[gltf_texture.imageIndex.value()].sampler;
            }

            if(gltf_material.emissiveTexture.has_value()) {
                const fastgltf::Texture& gltf_texture = asset->textures[gltf_material.emissiveTexture.value().textureIndex];
                gpu_material.albedo_image_id = model_data.images[gltf_texture.imageIndex.value()].image.default_view();
                gpu_material.albedo_sampler_id = model_data.images[gltf_texture.imageIndex.value()].sampler;
            }

            materials.push_back(gpu_material);
        }
            
        daxa::BufferId staging_materials_buffer = gpu_context->device.create_buffer({
            .size = sizeof(GPUMaterial) * asset->materials.size(),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "staging_materials_buffer",
        });
        cmd_recorder.destroy_buffer_deferred(staging_materials_buffer);

        std::memcpy(gpu_context->device.buffer_host_address(staging_materials_buffer).value(), materials.data(), sizeof(GPUMaterial) * asset->materials.size());

        cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
            .src_buffer = staging_materials_buffer,
            .dst_buffer = model_data.materials_buffer,
            .size = sizeof(GPUMaterial) * asset->materials.size()
        });

        gpu_context->device.submit_commands({ .command_lists = std::to_array({ cmd_recorder.complete_current_commands() })});
        gpu_context->device.wait_idle();
        gpu_context->device.wait_idle();

        return model_data;
    }
}
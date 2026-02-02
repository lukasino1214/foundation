#include "asset_manager.hpp"
#include <graphics/gpu_context.hpp>
#include <utils/thread_pool.hpp>

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
#include <graphics/helper.hpp>

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

    AssetManager::AssetManager(GPUContext* _gpu_context, AssetProcessor* _asset_processor, ThreadPool* _thread_pool, Scene* _scene) : gpu_context{_gpu_context}, asset_processor{_asset_processor}, thread_pool{_thread_pool}, scene{_scene} {
        gpu_materials = make_task_buffer(gpu_context, {
            sizeof(Material), 
            daxa::MemoryFlagBits::NONE, 
            "materials"
        });
        gpu_meshes = make_task_buffer(gpu_context, {
            sizeof(MeshGeometryData), 
            daxa::MemoryFlagBits::NONE, 
            "meshes"
        });
    }

    AssetManager::~AssetManager() {
        for(auto& mesh_manifest : mesh_manifests) {
            if(mesh_manifest.render_info.has_value()) {
                gpu_context->device.destroy_buffer(mesh_manifest.render_info->mesh_buffer);
                gpu_context->device.destroy_buffer(mesh_manifest.render_info->index_buffer);
            }
        }
    
        for(auto& texture_manifest : texture_manifests) {
            if(texture_manifest.render_info.has_value()) {
                gpu_context->device.destroy_image(texture_manifest.render_info->image_id);
            }
        }
    
        gpu_context->device.destroy_buffer(gpu_materials.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_meshes.get_state().buffers[0]);
    }
    

    void AssetManager::load_asset(const std::filesystem::path& file_path, Entity root_entity) {
        PROFILE_SCOPE;
        
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error(fmt::format("File does not exist with path: {}", file_path.string()));
        }

        AssetManifest* asset_manifest = {};
        u32 asset_manifest_index = {};
        bool already_loaded = false;
        if(auto it = asset_lookup.find(file_path); it != asset_lookup.end()) {
            asset_manifest_index = it->second;
            asset_manifest = &asset_manifests[asset_manifest_index];
            already_loaded = true;
        } else {
            asset_lookup[file_path] = s_cast<u32>(asset_manifests.size());
            asset_manifest_index = s_cast<u32>(asset_manifests.size());

            asset_manifests.push_back({
                .texture_manifest_offset = s_cast<u32>(texture_manifests.size()),
                .material_manifest_offset = s_cast<u32>(material_manifests.size()),
                .mesh_manifest_offset = s_cast<u32>(mesh_manifests.size()),
                .mesh_group_manifest_offset = s_cast<u32>(mesh_group_manifests.size()),
                .animation_manifest_offset = s_cast<u32>(animation_manifests.size()),
                .skeleton_manifest_offset = s_cast<u32>(skeleton_manifests.size()),
            });
            asset_manifest = &asset_manifests.back();

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
            asset_manifest->asset = std::make_unique<fastgltf::Asset>(std::move(fastgltf_asset.get()));
        }
        fastgltf::Asset* asset = asset_manifest->asset.get();

        if(!already_loaded) { 
            load_asset_to_manifests(asset, file_path, asset_manifest, asset_manifest_index);
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
                mesh_component.mesh_group_manifest_entry_index = asset_manifest->mesh_group_manifest_offset + s_cast<u32>(node.meshIndex.value());
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

            if(node.lightIndex.has_value()) {
                const fastgltf::Light& light = asset->lights[node.lightIndex.value()];
                
                const f32 LUMENS_PER_WATT = 683.0f;
                const f32 E_min = 1.0f;

                if(light.type == fastgltf::LightType::Point) {
                    parent_entity.add_component<PointLightComponent>();

                    auto& point_light_component = parent_entity.get_component<PointLightComponent>();
                    point_light_component.color = glm::vec3(light.color.x(), light.color.y(), light.color.z());
                    point_light_component.intensity = light.intensity;

                    f32 intensity = (light.intensity * 4.0f * glm::pi<f32>()) / LUMENS_PER_WATT;
                    point_light_component.range = light.range.value_or(std::sqrt(intensity / E_min));
                } else if(light.type == fastgltf::LightType::Spot) {
                    parent_entity.add_component<SpotLightComponent>();

                    auto& spot_light_component = parent_entity.get_component<SpotLightComponent>();
                    spot_light_component.color = glm::vec3(light.color.x(), light.color.y(), light.color.z());
                    spot_light_component.intensity = light.intensity;
                    spot_light_component.inner_cone_angle = light.innerConeAngle.value_or(0.0f);
                    spot_light_component.outer_cone_angle = light.outerConeAngle.value_or(std::numbers::pi_v<f32> / 4.0f);

                    f32 intensity = (light.intensity * glm::pi<f32>()) / LUMENS_PER_WATT;
                    spot_light_component.range = light.range.value_or(std::sqrt(intensity / E_min));
                }
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
    }

    void AssetManager::load_asset_to_manifests(fastgltf::Asset* asset, const std::filesystem::path& file_path, AssetManifest* asset_manifest, u32 asset_manifest_index) {
        for (u32 image_index = 0; image_index < asset->images.size(); ++image_index) {
            const fastgltf::Image& gltf_image = asset->images[image_index];

            texture_manifests.push_back(TextureManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_texture_index = image_index,
                .name = gltf_image.name.c_str(),
                .material_manifest_indices = {},
                .render_info = std::nullopt,
            });
        }

        auto gltf_texture_to_manifest_texture_index = [&](u32 const texture_index) -> std::optional<u32> {
            const bool gltf_texture_has_image_index = asset->textures.at(texture_index).imageIndex.has_value();
            if (!gltf_texture_has_image_index) { return std::nullopt; }
            else { return static_cast<u32>(asset->textures[texture_index].imageIndex.value()); }
        };

        material_manifests.reserve(material_manifests.size() + asset->materials.size());
        dirty_materials.reserve(dirty_materials.size() + asset->materials.size());
        for (u32 material_index = 0; material_index < static_cast<u32>(asset->materials.size()); material_index++) {
            const auto& material = asset->materials[material_index];
            dirty_materials.push_back(asset_manifest->material_manifest_offset + material_index);

            const bool has_normal_texture = material.normalTexture.has_value();
            const bool has_albedo_texture = material.pbrData.baseColorTexture.has_value();
            const bool has_roughness_metalness_texture = material.pbrData.metallicRoughnessTexture.has_value();
            const bool has_emissive_texture = material.emissiveTexture.has_value();
            std::optional<MaterialManifest::TextureInfo> albedo_texture_info = {};
            std::optional<MaterialManifest::TextureInfo> normal_texture_info = {};
            std::optional<MaterialManifest::TextureInfo> roughnes_metalness_info = {};
            std::optional<MaterialManifest::TextureInfo> emissive_info = {};
            if(has_albedo_texture) {
                const u32 texture_index = static_cast<u32>(material.pbrData.baseColorTexture.value().textureIndex);
                const auto has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    albedo_texture_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    texture_manifests[asset_manifest->texture_manifest_offset + albedo_texture_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .material_type = MaterialType::GltfAlbedo,
                        .asset_local_material_index = material_index,
                    });
                }
            }

            if(has_normal_texture) {
                const u32 texture_index = static_cast<u32>(material.normalTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    normal_texture_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    texture_manifests[asset_manifest->texture_manifest_offset + normal_texture_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .material_type = MaterialType::GltfNormal,
                        .asset_local_material_index = material_index,
                    });
                }
            }

            if(has_roughness_metalness_texture) {
                const u32 texture_index = static_cast<u32>(material.pbrData.metallicRoughnessTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    roughnes_metalness_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    texture_manifests[asset_manifest->texture_manifest_offset + roughnes_metalness_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .material_type = MaterialType::GltfRoughnessMetallic,
                        .asset_local_material_index = material_index,
                    });
                }
            }
            if(has_emissive_texture) {
                const u32 texture_index = static_cast<u32>(material.emissiveTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    emissive_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    texture_manifests[asset_manifest->texture_manifest_offset + emissive_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .material_type = MaterialType::GltfEmissive,
                        .asset_local_material_index = material_index,
                    });
                }
            }
            material_manifests.push_back(MaterialManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_material_local_index = material_index,
                .albedo_info = albedo_texture_info,
                .normal_info = normal_texture_info,
                .roughness_metalness_info = roughnes_metalness_info,
                .emissive_info = emissive_info,
                .albedo_factor = { material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1], material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3] },
                .metallic_factor = material.pbrData.metallicFactor,
                .roughness_factor = material.pbrData.roughnessFactor,
                .emissive_factor = { material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2] },
                .alpha_mode = s_cast<u32>(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided,
                .name = material.name.c_str(),
            });
        }

        mesh_group_manifests.reserve(asset_manifest->mesh_group_manifest_offset + asset->meshes.size());
        for(u32 mesh_group_index = 0; mesh_group_index < asset->meshes.size(); mesh_group_index++) {
            const fastgltf::Mesh& gltf_mesh = asset->meshes[mesh_group_index];
            
            u32 mesh_manifest_indices_offset = s_cast<u32>(mesh_manifests.size());
            mesh_manifests.reserve(mesh_manifests.size() + gltf_mesh.primitives.size());

            for(u32 mesh_index = 0; mesh_index < gltf_mesh.primitives.size(); mesh_index++) {
                const fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[mesh_index];
                
                mesh_manifests.push_back(MeshManifest {
                    .asset_manifest_index = asset_manifest_index,
                    .asset_local_mesh_group_index = mesh_group_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_material_index = gltf_primitive.materialIndex.has_value() ? std::make_optional(s_cast<u32>(gltf_primitive.materialIndex.value())) : std::nullopt,
                    .render_info = std::nullopt,
                });
            }

            mesh_group_manifests.push_back(MeshGroupManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_mesh_group_index = mesh_group_index,
                .asset_local_mesh_manifest_indices_offset = mesh_manifest_indices_offset - asset_manifest->mesh_manifest_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size()),
                .name = gltf_mesh.name.c_str(),
            });
        }

        animation_manifests.reserve(animation_manifests.size() + asset->animations.size());
        for(u32 animation_index = 0; animation_index < asset->animations.size(); animation_index++) {
            const fastgltf::Animation& gltf_animation = asset->animations[animation_index];

            std::vector<AnimationChannel> channels = {};
            channels.reserve(gltf_animation.channels.size());
            for(const fastgltf::AnimationChannel& gltf_channel : gltf_animation.channels) {
                AnimationChannel channel = {};
                channel.sampler_index = s_cast<u32>(gltf_channel.samplerIndex);
                channel.path = s_cast<AnimationPath>(s_cast<u8>(gltf_channel.path) - 1); // dont ask me why
                if (gltf_channel.nodeIndex.has_value()) {
                    channel.node_index = s_cast<u32>(gltf_channel.nodeIndex.value());
                }
                channels.push_back(channel);
            }

            f32 min_timestamp = std::numeric_limits<f32>::max();
            f32 max_timestamp = std::numeric_limits<f32>::lowest();

            std::vector<AnimationSampler> samplers = {};
            samplers.reserve(gltf_animation.samplers.size());
            for(const fastgltf::AnimationSampler& gltf_sampler : gltf_animation.samplers) {
                auto vec_to_floats = [&](usize accessor_index) -> std::vector<f32> {
                    const auto& value_accessor = asset->accessors[accessor_index];
                    std::vector<f32> values = {};
                    switch (value_accessor.type) {
                        case fastgltf::AccessorType::Vec3: {
                            std::vector<glm::vec3> data = load_accessor_data_from_file<glm::vec3, false>(file_path.parent_path(), *asset, value_accessor);
                            values.resize(3 * data.size());
                            std::memcpy(values.data(), data.data(), 3 * data.size() * sizeof(f32));
                            break;
                        }
                        case fastgltf::AccessorType::Vec4: {
                            std::vector<glm::vec4> data = load_accessor_data_from_file<glm::vec4, false>(file_path.parent_path(), *asset, value_accessor);
                            values.resize(4 * data.size());
                            std::memcpy(values.data(), data.data(), 4 * data.size() * sizeof(f32));
                            break;
                        }
                        case fastgltf::AccessorType::Scalar: {
                            std::vector<f32> data = load_accessor_data_from_file<f32, false>(file_path.parent_path(), *asset, value_accessor);
                            values.resize(data.size());
                            std::memcpy(values.data(), data.data(), data.size() * sizeof(f32));
                            break;
                        }
                        default: {
                            throw std::runtime_error("something went wrong"); 
                        }
                    }
                    return values;
                };

                AnimationSampler sampler = {};
                sampler.timestamps = vec_to_floats(gltf_sampler.inputAccessor);
                sampler.values = vec_to_floats(gltf_sampler.outputAccessor);
                sampler.interpolation = static_cast<AnimationInterpolation>(s_cast<u8>(gltf_sampler.interpolation));
                
                min_timestamp = glm::min(min_timestamp, sampler.timestamps.front());
                max_timestamp = glm::max(max_timestamp, sampler.timestamps.back());
                
                samplers.push_back(sampler);
            }

            animation_manifests.push_back(AnimationManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_animation_index = animation_index,
                .channels = channels,
                .samplers = samplers,
                .min_timestamp = min_timestamp,
                .max_timestamp = max_timestamp,
                .name = gltf_animation.name.c_str(),
            });
        }

        skeleton_manifests.reserve(skeleton_manifests.size() + asset->skins.size());
        for(u32 skin_index = 0; skin_index < asset->skins.size(); skin_index++) {
            const fastgltf::Skin& gltf_skin = asset->skins[skin_index];

            std::vector<u32> joint_node_indices = {};
            joint_node_indices.reserve(gltf_skin.joints.size());
            for(usize joint_node_index : gltf_skin.joints) {
                joint_node_indices.push_back(s_cast<u32>(joint_node_index));
            }

            std::vector<glm::mat4> inverse_bind_matrices = {};
            if(gltf_skin.inverseBindMatrices.has_value()) {
                inverse_bind_matrices = load_accessor_data_from_file<glm::mat4, false>(file_path.parent_path(), *asset, asset->accessors[gltf_skin.inverseBindMatrices.value()]);
            }

            skeleton_manifests.push_back(SkeletonManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_skeleton_index = skin_index,
                .inverse_bind_matrices = inverse_bind_matrices,
                .root_node_index = gltf_skin.skeleton.has_value() ? std::make_optional(s_cast<u32>(gltf_skin.skeleton.value())) : std::nullopt,
                .joint_node_indices = joint_node_indices,
                .name = gltf_skin.name.c_str(),
            });
        }

        asset_manifest->texture_manifest_size = s_cast<u32>(texture_manifests.size()) - asset_manifest->texture_manifest_offset;
        asset_manifest->material_manifest_size = s_cast<u32>(material_manifests.size()) - asset_manifest->material_manifest_offset;
        asset_manifest->mesh_manifest_size = s_cast<u32>(mesh_manifests.size()) - asset_manifest->mesh_manifest_offset;
        asset_manifest->mesh_group_manifest_size = s_cast<u32>(mesh_group_manifests.size()) - asset_manifest->mesh_group_manifest_offset;
        asset_manifest->animation_manifest_size = s_cast<u32>(animation_manifests.size()) - asset_manifest->animation_manifest_offset;
        asset_manifest->skeleton_manifest_size = s_cast<u32>(skeleton_manifests.size()) - asset_manifest->skeleton_manifest_offset;

        struct LoadMeshTask : Task {
            struct TaskInfo {
                LoadMeshInfo load_info = {};
                AssetProcessor * asset_processor = {};
                u32 manifest_index = {};
            };

            TaskInfo info = {};
            LoadMeshTask(const TaskInfo& info) : info{info} { chunk_count = 1; }

            virtual void callback(u32, u32) override {
                info.asset_processor->process_mesh_load(info.load_info);
            };
        };

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);
            const auto& mesh_group_manifest = mesh_group_manifests[asset_manifest->mesh_group_manifest_offset + mesh_index];
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                    .load_info = {
                        .asset = asset,
                        .asset_path = file_path,
                        .asset_manifest_index = asset_manifest_index,
                        .mesh_manifest_index = mesh_group_manifest.asset_local_mesh_manifest_indices_offset + primitive_index,
                        .asset_local_mesh_group_index = mesh_index,
                        .asset_local_mesh_index = primitive_index,
                    },
                    .asset_processor = asset_processor,
                }), TaskPriority::LOW);
            }
        }

        struct LoadTextureTask : Task {
            struct TaskInfo {
                LoadTextureInfo load_info = {};
                AssetProcessor * asset_processor = {};
                u32 manifest_index = {};
            };

            TaskInfo info = {};
            explicit LoadTextureTask(TaskInfo const & info) : info{info} { chunk_count = 1; }

            virtual void callback(u32, u32) override {
                info.asset_processor->process_texture_load(info.load_info);
            };
        };

        for (u32 gltf_texture_index = 0; gltf_texture_index < asset->images.size(); gltf_texture_index++) {
            const auto texture_manifest_index = asset_manifest->texture_manifest_offset + gltf_texture_index;
            const auto& texture_manifest_entry = texture_manifests[texture_manifest_index];

            bool used_as_albedo = false;
            for (const auto& material_manifest_index : texture_manifest_entry.material_manifest_indices) {
                used_as_albedo |= material_manifest_index.material_type == MaterialType::GltfAlbedo;
                used_as_albedo |= material_manifest_index.material_type == MaterialType::GltfEmissive;
            }

            if (!texture_manifest_entry.material_manifest_indices.empty()) {
                thread_pool->async_dispatch(
                std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                    .load_info = {
                        .asset = asset,
                        .asset_path = file_path,
                        .load_as_srgb = used_as_albedo,
                        .asset_manifest_index = asset_manifest_index,
                        .texture_manifest_index = gltf_texture_index,
                    },
                    .asset_processor = asset_processor,
                }), TaskPriority::LOW);
            }
        }
    }

    void AssetManager::unload_asset(const std::filesystem::path& file_path) {
        if(auto it = asset_lookup.find(file_path); it != asset_lookup.end()) {
            unload_asset(it->second);
        }
    }

    void AssetManager::unload_asset(u32 /* asset_index */) {
        
    }

    void AssetManager::update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos) {
        PROFILE_SCOPE;

        reallocate_buffer(gpu_context, cmd_recorder, gpu_materials, s_cast<u32>(material_manifests.size() * sizeof(Material)));
        reallocate_buffer(gpu_context, cmd_recorder, gpu_meshes, s_cast<u32>(mesh_manifests.size() * sizeof(MeshGeometryData)));

        std::vector<MeshGeometryData> meshes = {};
        std::vector<u32> mesh_indices = {};
        meshes.reserve(upload_infos.meshes.size());
        mesh_indices.reserve(upload_infos.meshes.size());
        for(const MeshUploadInfo& mesh_upload_info : upload_infos.meshes) {
            const AssetManifest& asset_manifest = asset_manifests[mesh_upload_info.asset_manifest_index];
            u32 mesh_manifest_manifest_index = asset_manifest.mesh_manifest_offset + mesh_upload_info.mesh_manifest_index;
            MeshManifest& mesh_manifest = mesh_manifests[mesh_manifest_manifest_index];
            MeshGeometryData mesh_geometry_info = mesh_upload_info.mesh_geometry_info;
            mesh_geometry_info.material_index = mesh_manifest.asset_local_material_index.has_value() ? asset_manifest.material_manifest_offset + mesh_manifest.asset_local_material_index.value() : INVALID_ID;

            mesh_manifest.render_info = {
                .mesh_buffer = mesh_upload_info.mesh_buffer,
                .index_buffer = mesh_upload_info.index_buffer,
                .mesh = mesh_geometry_info,
                .vertex_count = mesh_geometry_info.vertex_count,
                .index_count = mesh_geometry_info.index_count,
            };

            meshes.push_back(mesh_geometry_info);
            mesh_indices.push_back(asset_manifest.mesh_manifest_offset + mesh_upload_info.mesh_manifest_index);
        }

        if(!meshes.empty()) {
            daxa::BufferId staging_meshes_buffer = gpu_context->device.create_buffer({
                .size = sizeof(MeshGeometryData) * meshes.size(),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging_meshes_buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_meshes_buffer);

            std::memcpy(gpu_context->device.buffer_host_address(staging_meshes_buffer).value(), meshes.data(), sizeof(MeshGeometryData) * meshes.size());

            u32 mesh_upload_index = 0;
            for(u32 gpu_mesh_index : mesh_indices) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_meshes_buffer,
                    .dst_buffer = gpu_meshes.get_state().buffers[0],
                    .src_offset = sizeof(MeshGeometryData) * mesh_upload_index++,
                    .dst_offset = sizeof(MeshGeometryData) * gpu_mesh_index,
                    .size = sizeof(MeshGeometryData)
                });
            }
        }

        for(const TextureUploadInfo& texture_upload_info : upload_infos.textures) {
            const AssetManifest& asset_manifest = asset_manifests[texture_upload_info.asset_manifest_index];
            TextureManifest& texture_manifest = texture_manifests[asset_manifest.texture_manifest_offset + texture_upload_info.texture_manifest_index];
            
            texture_manifest.render_info = {
                .image_id = texture_upload_info.image,
                .sampler_id = texture_upload_info.sampler,
            };

            for(auto& material_using_texture_info : texture_manifest.material_manifest_indices) {
                MaterialManifest& material_manifest = material_manifests[asset_manifest.material_manifest_offset + material_using_texture_info.asset_local_material_index];
                if (material_using_texture_info.material_type == MaterialType::GltfAlbedo) {
                    if (material_manifest.albedo_info.has_value()) {
                        material_manifest.albedo_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if (material_using_texture_info.material_type == MaterialType::GltfNormal) {
                    if (material_manifest.normal_info.has_value()) {
                        material_manifest.normal_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }
                
                if (material_using_texture_info.material_type == MaterialType::GltfRoughnessMetallic) {
                    if (material_manifest.roughness_metalness_info.has_value()) {
                        material_manifest.roughness_metalness_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if (material_using_texture_info.material_type == MaterialType::GltfEmissive) {
                    if (material_manifest.emissive_info.has_value()) {
                        material_manifest.emissive_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if (std::find(dirty_materials.begin(), dirty_materials.end(), material_using_texture_info.asset_local_material_index) == dirty_materials.end()) {
                    dirty_materials.push_back(asset_manifest.material_manifest_offset + material_using_texture_info.asset_local_material_index);
                }
            }
        }

        if(!dirty_materials.empty()) {
            std::vector<Material> materials = {};
            materials.reserve(dirty_materials.size());
            materials.reserve(dirty_materials.size());
            for(u32 material_index : dirty_materials) {
                const MaterialManifest& material_manifest = material_manifests[material_index];
                const AssetManifest& local_asset_manifest = asset_manifests[material_manifest.asset_manifest_index];

                Material gpu_material = {};
                gpu_material.albedo_factor = material_manifest.albedo_factor;
                gpu_material.metallic_factor = material_manifest.metallic_factor;
                gpu_material.roughness_factor = material_manifest.roughness_factor;
                gpu_material.emissive_factor = material_manifest.emissive_factor;
                gpu_material.alpha_mode = material_manifest.alpha_mode;
                gpu_material.alpha_cutoff = material_manifest.alpha_cutoff;
                gpu_material.double_sided = static_cast<b32>(material_manifest.double_sided);

                if (material_manifest.albedo_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.albedo_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.albedo_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.albedo_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                if (material_manifest.normal_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.normal_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.normal_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.normal_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                if (material_manifest.roughness_metalness_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.roughness_metalness_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.roughness_metalness_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.roughness_metalness_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                if (material_manifest.emissive_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.emissive_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.emissive_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.emissive_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                materials.push_back(gpu_material);
            }

            daxa::BufferId staging_materials_buffer = gpu_context->device.create_buffer({
                .size = sizeof(Material) * materials.size(),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging_materials_buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_materials_buffer);

            std::memcpy(gpu_context->device.buffer_host_address(staging_materials_buffer).value(), materials.data(), sizeof(Material) * materials.size());

            u32 material_upload_index = 0;
            for(u32 gpu_material_index : dirty_materials) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_materials_buffer,
                    .dst_buffer = gpu_materials.get_state().buffers[0],
                    .src_offset = sizeof(Material) * material_upload_index++,
                    .dst_offset = sizeof(Material) * gpu_material_index,
                    .size = sizeof(Material)
                });
            }
            dirty_materials.clear();
        }
    }
}
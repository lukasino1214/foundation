#include "gltf_loader.hpp"

#include "asset_processor.hpp"

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

#include <ecs/entity.hpp>
#include <ecs/scene.hpp>

#include <math/math.hpp>

namespace foundation {
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
    auto load_accessor_data_from_file(const std::filesystem::path& root_path, const fastgltf::Asset& gltf_asset, const fastgltf::Accessor& accessor) -> std::vector<ElemT> {
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

    struct GLTFAsset : Asset {
        std::unique_ptr<fastgltf::Asset> asset = {};
        std::filesystem::path asset_path = {};

        GLTFAsset(std::unique_ptr<fastgltf::Asset>&& _asset, const std::filesystem::path& _asset_path) : asset{std::move(_asset)}, asset_path{_asset_path} {}
        virtual ~GLTFAsset() override = default;

        virtual void load_entites(const AssetManifest& asset_manifest, Scene* scene, Entity& root_entity) override {
            PROFILE_SCOPE;
            
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
                    mesh_component.mesh_group_manifest_entry_index = asset_manifest.mesh_group_manifest_offset + s_cast<u32>(node.meshIndex.value());
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

        virtual auto load_image(u32 image_index) -> std::variant<LoadedRawImageData, LoadedImageInfo> override {
            PROFILE_SCOPE;
            
            const fastgltf::Image& gltf_image = asset->images[image_index];
            if(const fastgltf::sources::URI* uri = std::get_if<fastgltf::sources::URI>(&gltf_image.data)) {
                return LoadedImageInfo {
                    .file_path = asset_path.parent_path() / uri->uri.fspath(),
                    .name = gltf_image.name.c_str(),
                };
            } else {
                BufferData buffer_data = fastgltf_read_data(asset_path, *asset, gltf_image.data);

                return LoadedRawImageData {
                    .loaded_data = std::move(buffer_data.loaded_data),
                    .data_span = buffer_data.data_span,
                    .name = gltf_image.name.c_str(),
                };
            }
        }

        virtual auto load_mesh(u32 mesh_group_index, u32 mesh_index) -> LoadedMesh override {
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
                loaded_mesh.positions = load_accessor_data_from_file<glm::vec3, false>(asset_path.parent_path(), *asset, asset->accessors[position_attribute_iter->accessorIndex]);
            }

            u32 vertex_count = s_cast<u32>(loaded_mesh.positions.size());

            const fastgltf::Attribute* normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
            if(normal_attribute_iter != gltf_primitive.attributes.end()) {
                loaded_mesh.normals = load_accessor_data_from_file<glm::vec3, false>(asset_path.parent_path(), *asset, asset->accessors[normal_attribute_iter->accessorIndex]);
            } else {
                fill(loaded_mesh.normals, vertex_count);
            }

            const fastgltf::Attribute* uv_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
            if(uv_attribute_iter != gltf_primitive.attributes.end()) {
                loaded_mesh.uvs = load_accessor_data_from_file<glm::vec2, false>(asset_path.parent_path(), *asset, asset->accessors[uv_attribute_iter->accessorIndex]);
            } else {
                fill(loaded_mesh.uvs, vertex_count);
            }

            if(gltf_primitive.indicesAccessor.has_value()) {
                loaded_mesh.indices = load_accessor_data_from_file<u32, true>(asset_path.parent_path(), *asset, asset->accessors[gltf_primitive.indicesAccessor.value()]);
            }

            loaded_mesh.name = fmt::format("mesh: {} {} {}", asset_path.filename().string().c_str(), gltf_mesh.name.c_str(), mesh_index);
            return loaded_mesh;
        }
    };

    auto GLTFLoader::load(const std::filesystem::path& file_path, u32 asset_manifest_index) -> LoadedAsset {
        PROFILE_SCOPE;
        
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

        LoadedAsset loaded_asset = {};
        loaded_asset.texture_manifests.reserve(asset->images.size());
        for(u32 image_index = 0; image_index < asset->images.size(); ++image_index) {
            const fastgltf::Image& gltf_image = asset->images[image_index];

            loaded_asset.texture_manifests.push_back(TextureManifest {
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
            else { return s_cast<u32>(asset->textures[texture_index].imageIndex.value()); }
        };

        loaded_asset.material_manifests.reserve(asset->materials.size());
        for (u32 material_index = 0; material_index < s_cast<u32>(asset->materials.size()); material_index++) {
            const auto& material = asset->materials[material_index];

            const bool has_normal_texture = material.normalTexture.has_value();
            const bool has_albedo_texture = material.pbrData.baseColorTexture.has_value();
            const bool has_roughness_metalness_texture = material.pbrData.metallicRoughnessTexture.has_value();
            const bool has_emissive_texture = material.emissiveTexture.has_value();
            std::optional<MaterialManifest::TextureInfo> albedo_texture_info = {};
            std::optional<MaterialManifest::TextureInfo> normal_texture_info = {};
            std::optional<MaterialManifest::TextureInfo> roughnes_metalness_info = {};
            std::optional<MaterialManifest::TextureInfo> emissive_info = {};
            if(has_albedo_texture) {
                const u32 texture_index = s_cast<u32>(material.pbrData.baseColorTexture.value().textureIndex);
                const auto has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    albedo_texture_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    loaded_asset.texture_manifests[albedo_texture_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .texture_type = TextureType::GltfAlbedo,
                        .asset_local_material_index = material_index,
                    });
                }
            }

            if(has_normal_texture) {
                const u32 texture_index = s_cast<u32>(material.normalTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    normal_texture_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    loaded_asset.texture_manifests[normal_texture_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .texture_type = TextureType::GltfNormal,
                        .asset_local_material_index = material_index,
                    });
                }
            }

            if(has_roughness_metalness_texture) {
                const u32 texture_index = s_cast<u32>(material.pbrData.metallicRoughnessTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    roughnes_metalness_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    loaded_asset.texture_manifests[roughnes_metalness_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .texture_type = TextureType::GltfRoughnessMetallic,
                        .asset_local_material_index = material_index,
                    });
                }
            }
            if(has_emissive_texture) {
                const u32 texture_index = s_cast<u32>(material.emissiveTexture.value().textureIndex);
                const bool has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if(has_index) {
                    emissive_info = MaterialManifest::TextureInfo {
                        .asset_local_texture_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    loaded_asset.texture_manifests[emissive_info->asset_local_texture_index].material_manifest_indices.push_back(TextureManifest::MaterialManifestIndex {
                        .texture_type = TextureType::GltfEmissive,
                        .asset_local_material_index = material_index,
                    });
                }
            }
            loaded_asset.material_manifests.push_back(MaterialManifest {
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
                .material_type = s_cast<MaterialType>(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided,
                .name = material.name.c_str(),
            });
        }

        loaded_asset.mesh_group_manifests.reserve(asset->meshes.size());
        for(u32 mesh_group_index = 0; mesh_group_index < asset->meshes.size(); mesh_group_index++) {
            const fastgltf::Mesh& gltf_mesh = asset->meshes[mesh_group_index];
            
            u32 mesh_manifest_indices_offset = s_cast<u32>(loaded_asset.mesh_manifests.size());
            loaded_asset.mesh_manifests.reserve(loaded_asset.mesh_manifests.size() + gltf_mesh.primitives.size());

            for(u32 mesh_index = 0; mesh_index < gltf_mesh.primitives.size(); mesh_index++) {
                const fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[mesh_index];
                
                loaded_asset.mesh_manifests.push_back(MeshManifest {
                    .asset_manifest_index = asset_manifest_index,
                    .asset_local_mesh_group_index = mesh_group_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_material_index = gltf_primitive.materialIndex.has_value() ? std::make_optional(s_cast<u32>(gltf_primitive.materialIndex.value())) : std::nullopt,
                    .render_info = std::nullopt,
                });
            }

            loaded_asset.mesh_group_manifests.push_back(MeshGroupManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_mesh_group_index = mesh_group_index,
                .asset_local_mesh_manifest_indices_offset = mesh_manifest_indices_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size()),
                .name = gltf_mesh.name.c_str(),
            });
        }

        loaded_asset.animation_manifests.reserve(asset->animations.size());
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
                sampler.interpolation = s_cast<AnimationInterpolation>(s_cast<u8>(gltf_sampler.interpolation));
                
                min_timestamp = glm::min(min_timestamp, sampler.timestamps.front());
                max_timestamp = glm::max(max_timestamp, sampler.timestamps.back());
                
                samplers.push_back(sampler);
            }

            loaded_asset.animation_manifests.push_back(AnimationManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_animation_index = animation_index,
                .channels = channels,
                .samplers = samplers,
                .min_timestamp = min_timestamp,
                .max_timestamp = max_timestamp,
                .name = gltf_animation.name.c_str(),
            });
        }

        loaded_asset.skeleton_manifests.reserve(asset->skins.size());
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

            loaded_asset.skeleton_manifests.push_back(SkeletonManifest {
                .asset_manifest_index = asset_manifest_index,
                .asset_local_skeleton_index = skin_index,
                .inverse_bind_matrices = inverse_bind_matrices,
                .root_node_index = gltf_skin.skeleton.has_value() ? std::make_optional(s_cast<u32>(gltf_skin.skeleton.value())) : std::nullopt,
                .joint_node_indices = joint_node_indices,
                .name = gltf_skin.name.c_str(),
            });
        }

        loaded_asset.asset = std::make_unique<GLTFAsset>(std::move(asset), file_path);
        return loaded_asset;
    }
}
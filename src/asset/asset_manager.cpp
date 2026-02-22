#include "asset_manager.hpp"
#include <graphics/gpu_context.hpp>
#include <utils/thread_pool.hpp>

#include <ecs/entity.hpp>
#include <ecs/scene.hpp>

#include <math/math.hpp>
#include "asset_processor.hpp"
#include <graphics/helper.hpp>

#include "gltf_loader.hpp"

namespace foundation {
    AssetManager::AssetManager(GPUContext* _gpu_context, AssetProcessor* _asset_processor, ThreadPool* _thread_pool, Scene* _scene) : gpu_context{_gpu_context}, asset_processor{_asset_processor}, thread_pool{_thread_pool}, scene{_scene} {
        gpu_materials = make_task_buffer(gpu_context, {
            sizeof(Material), 
            daxa::MemoryFlagBits::NONE, 
            "materials"
        });
    }

    AssetManager::~AssetManager() {
        for(auto& mesh_manifest : mesh_manifests) {
            if(mesh_manifest.render_info.has_value()) {
                gpu_context->device.destroy_buffer(mesh_manifest.render_info->mesh_buffer);
                gpu_context->device.destroy_buffer(mesh_manifest.render_info->index_buffer);
                gpu_context->device.destroy_blas(mesh_manifest.render_info->blas);
            }
        }

        for(auto& texture_manifest : texture_manifests) {
            if(texture_manifest.render_info.has_value()) {
                gpu_context->device.destroy_image(texture_manifest.render_info->image_id);
            }
        }

        gpu_context->device.destroy_buffer(gpu_materials.get_state().buffers[0]);
    }

    void AssetManager::load_asset(const std::filesystem::path& file_path, Entity root_entity) {
        PROFILE_SCOPE;

        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error(fmt::format("File does not exist with path: {}", file_path.string()));
        }

        AssetManifest* asset_manifest = {};
        u32 asset_manifest_index = {};
        if(auto it = asset_lookup.find(file_path); it != asset_lookup.end()) {
            asset_manifest_index = it->second;
            asset_manifest = &asset_manifests[asset_manifest_index];
        } else {
            asset_lookup[file_path] = s_cast<u32>(asset_manifests.size());
            asset_manifest_index = s_cast<u32>(asset_manifests.size());

            LoadedAsset loaded_asset = GLTFLoader::load(file_path, asset_manifest_index);

            asset_manifests.push_back({
                .texture_manifest_offset = s_cast<u32>(texture_manifests.size()),
                .material_manifest_offset = s_cast<u32>(material_manifests.size()),
                .mesh_manifest_offset = s_cast<u32>(mesh_manifests.size()),
                .mesh_group_manifest_offset = s_cast<u32>(mesh_group_manifests.size()),
                .animation_manifest_offset = s_cast<u32>(animation_manifests.size()),
                .skeleton_manifest_offset = s_cast<u32>(skeleton_manifests.size()),
                .texture_manifest_size = s_cast<u32>(loaded_asset.texture_manifests.size()),
                .material_manifest_size = s_cast<u32>(loaded_asset.material_manifests.size()),
                .mesh_manifest_size = s_cast<u32>(loaded_asset.mesh_manifests.size()),
                .mesh_group_manifest_size = s_cast<u32>(loaded_asset.mesh_group_manifests.size()),
                .animation_manifest_size = s_cast<u32>(loaded_asset.animation_manifests.size()),
                .skeleton_manifest_size = s_cast<u32>(loaded_asset.skeleton_manifests.size()),
                .asset = std::move(loaded_asset.asset),
            });
            asset_manifest = &asset_manifests.back();

            dirty_materials.reserve(dirty_materials.size() + loaded_asset.material_manifests.size());
            for(u32 material_index = 0; material_index < loaded_asset.material_manifests.size(); material_index++) {
                dirty_materials.push_back(asset_manifest->material_manifest_offset + material_index);
            }

            texture_manifests.insert(texture_manifests.end(), loaded_asset.texture_manifests.begin(), loaded_asset.texture_manifests.end());
            material_manifests.insert(material_manifests.end(), loaded_asset.material_manifests.begin(), loaded_asset.material_manifests.end());
            mesh_manifests.insert(mesh_manifests.end(), loaded_asset.mesh_manifests.begin(), loaded_asset.mesh_manifests.end());
            mesh_group_manifests.insert(mesh_group_manifests.end(), loaded_asset.mesh_group_manifests.begin(), loaded_asset.mesh_group_manifests.end());
            animation_manifests.insert(animation_manifests.end(), loaded_asset.animation_manifests.begin(), loaded_asset.animation_manifests.end());
            skeleton_manifests.insert(skeleton_manifests.end(), loaded_asset.skeleton_manifests.begin(), loaded_asset.skeleton_manifests.end());

            struct LoadMeshTask : Task {
                struct TaskInfo {
                    LoadMeshInfo load_info = {};
                    AssetProcessor* asset_processor = {};
                    u32 manifest_index = {};
                };

                TaskInfo info = {};
                explicit LoadMeshTask(const TaskInfo& info) : info{info} { chunk_count = 1; }

                virtual void callback(u32, u32) override {
                    info.asset_processor->process_mesh_load(info.load_info);
                };
            };

            for(u32 mesh_group_index = 0; mesh_group_index < loaded_asset.mesh_group_manifests.size(); mesh_group_index++) {
                const MeshGroupManifest& mesh_group_manifest = loaded_asset.mesh_group_manifests[mesh_group_index];
                for(u32 mesh_index = 0; mesh_index < mesh_group_manifest.mesh_count; mesh_index++) {
                    const MeshManifest& mesh_manifest = loaded_asset.mesh_manifests[mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index];

                    bool has_alpha = false;
                    if(mesh_manifest.asset_local_material_index.has_value()) {
                        has_alpha = loaded_asset.material_manifests[mesh_manifest.asset_local_material_index.value()].material_type != MaterialType::Opaque;
                    }

                    thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo {
                        .load_info = {
                            .asset = asset_manifest->asset.get(),
                            .asset_path = file_path,
                            .asset_manifest_index = asset_manifest_index,
                            .mesh_manifest_index = mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index,
                            .asset_local_mesh_group_index = mesh_group_index,
                            .asset_local_mesh_index = mesh_index,
                            .has_alpha = has_alpha,
                        },
                        .asset_processor = asset_processor,
                    }), TaskPriority::LOW);
                }
            }

            struct LoadTextureTask : Task {
                struct TaskInfo {
                    LoadTextureInfo load_info = {};
                    AssetProcessor* asset_processor = {};
                    u32 manifest_index = {};
                };

                TaskInfo info = {};
                explicit LoadTextureTask(const TaskInfo& info) : info{info} { chunk_count = 1; }

                virtual void callback(u32, u32) override {
                    info.asset_processor->process_texture_load(info.load_info);
                };
            };

            for(u32 texture_index = 0; texture_index < loaded_asset.texture_manifests.size(); texture_index++) {
                const auto& texture_manifest_entry = loaded_asset.texture_manifests[texture_index];

                bool used_as_albedo = false;
                TextureType texture_type = TextureType::None;
                for(const auto& material_manifest_index : texture_manifest_entry.material_manifest_indices) {
                    used_as_albedo |= material_manifest_index.texture_type == TextureType::GltfAlbedo;
                    used_as_albedo |= material_manifest_index.texture_type == TextureType::GltfEmissive;
                    if(texture_type == TextureType::None) {
                        texture_type = material_manifest_index.texture_type;
                    }
                }

                if(!texture_manifest_entry.material_manifest_indices.empty()) {
                    thread_pool->async_dispatch(std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo {
                        .load_info = {
                            .asset = asset_manifest->asset.get(),
                            .asset_path = file_path,
                            .load_as_srgb = used_as_albedo,
                            .texture_type = texture_type,
                            .asset_manifest_index = asset_manifest_index,
                            .texture_manifest_index = texture_index,
                        },
                        .asset_processor = asset_processor,
                    }), TaskPriority::LOW);
                }
            }
        }

        asset_manifest->asset->load_entites(*asset_manifest, scene, root_entity);
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

        for(const MeshUploadInfo& mesh_upload_info : upload_infos.meshes) {
            const AssetManifest& asset_manifest = asset_manifests[mesh_upload_info.asset_manifest_index];
            u32 mesh_manifest_manifest_index = asset_manifest.mesh_manifest_offset + mesh_upload_info.mesh_manifest_index;
            MeshManifest& mesh_manifest = mesh_manifests[mesh_manifest_manifest_index];
            MeshGeometryData mesh_geometry_data = mesh_upload_info.mesh_geometry_data;
            mesh_geometry_data.material_index = mesh_manifest.asset_local_material_index.has_value() ? asset_manifest.material_manifest_offset + mesh_manifest.asset_local_material_index.value() : INVALID_ID;

            mesh_manifest.render_info = {
                .mesh_buffer = mesh_upload_info.mesh_buffer,
                .index_buffer = mesh_upload_info.index_buffer,
                .blas = mesh_upload_info.blas,
                .mesh = mesh_geometry_data,
                .vertex_count = mesh_geometry_data.vertex_count,
                .index_count = mesh_geometry_data.index_count,
                .meshlet_count = mesh_geometry_data.meshlet_count,
            };
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
                if(material_using_texture_info.texture_type == TextureType::GltfAlbedo) {
                    if(material_manifest.albedo_info.has_value()) {
                        material_manifest.albedo_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if(material_using_texture_info.texture_type == TextureType::GltfNormal) {
                    if(material_manifest.normal_info.has_value()) {
                        material_manifest.normal_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if(material_using_texture_info.texture_type == TextureType::GltfRoughnessMetallic) {
                    if(material_manifest.roughness_metalness_info.has_value()) {
                        material_manifest.roughness_metalness_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if(material_using_texture_info.texture_type == TextureType::GltfEmissive) {
                    if(material_manifest.emissive_info.has_value()) {
                        material_manifest.emissive_info->asset_local_texture_index = texture_upload_info.texture_manifest_index;
                    }
                }

                if(std::find(dirty_materials.begin(), dirty_materials.end(), material_using_texture_info.asset_local_material_index) == dirty_materials.end()) {
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
                gpu_material.alpha_mode = s_cast<u32>(material_manifest.material_type);
                gpu_material.alpha_cutoff = material_manifest.alpha_cutoff;
                gpu_material.double_sided = s_cast<b32>(material_manifest.double_sided);
                gpu_material.normal_image_compressed = 0;

                if(material_manifest.albedo_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.albedo_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.albedo_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.albedo_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                if(material_manifest.normal_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.normal_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.normal_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.normal_sampler_id = texture_manifest.render_info.value().sampler_id;

                        if(gpu_context->device.image_info(texture_manifest.render_info.value().image_id).value().format != daxa::Format::R8G8B8A8_UNORM) {
                            gpu_material.normal_image_compressed = 1;
                        }
                    }
                }

                if(material_manifest.roughness_metalness_info.has_value()) {
                    const TextureManifest& texture_manifest = texture_manifests[local_asset_manifest.texture_manifest_offset + material_manifest.roughness_metalness_info.value().asset_local_texture_index];
                    if(texture_manifest.render_info.has_value()) {
                        gpu_material.roughness_metalness_image_id = texture_manifest.render_info.value().image_id.default_view();
                        gpu_material.roughness_metalness_sampler_id = texture_manifest.render_info.value().sampler_id;
                    }
                }

                if(material_manifest.emissive_info.has_value()) {
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

            // why the hell is this necessary?
            cmd_recorder.pipeline_barrier(daxa::BarrierInfo {
                .src_access = daxa::AccessConsts::COPY_WRITE,
                .dst_access = daxa::AccessConsts::COPY_WRITE,
            });

            u32 material_upload_index = 0;
            for(u32 gpu_material_index : dirty_materials) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_materials_buffer,
                    .dst_buffer = gpu_materials.get_state().buffers[0],
                    .src_offset = sizeof(Material) * material_upload_index++,
                    .dst_offset = sizeof(Material) * gpu_material_index,
                    .size = sizeof(Material),
                });

                // why the hell is this necessary?
                cmd_recorder.pipeline_barrier(daxa::BarrierInfo {
                    .src_access = daxa::AccessConsts::COPY_WRITE,
                    .dst_access = daxa::AccessConsts::COPY_WRITE,
                });
            }
            dirty_materials.clear();
        }
    }
} // namespace foundation
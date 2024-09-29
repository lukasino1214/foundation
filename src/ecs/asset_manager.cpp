#include "asset_manager.hpp"
#include "ecs/components.hpp"
#include "graphics/helper.hpp"

#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>
#include <utils/byte_utils.hpp>
#include <fstream>
#include <random>
#include <stb_image.h>
#include <nvtt/nvtt.h>
#include <zstd.h>

namespace foundation {
    AssetManager::AssetManager(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {
        gpu_meshes = make_task_buffer(context, {
            sizeof(Mesh), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "meshes"
        });

        gpu_materials = make_task_buffer(context, {
            sizeof(Material), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "materials"
        });

        gpu_mesh_groups = make_task_buffer(context, {
            sizeof(MeshGroup),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "mesh groups"
        });

        gpu_mesh_indices = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "mesh indices"
        });

        gpu_meshlet_data = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlet data"
        });

        gpu_culled_meshlet_data = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "culled meshlet data"
        });

        gpu_meshlet_index_buffer = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlet index buffer"
        });
    }
    AssetManager::~AssetManager() {
        context->destroy_buffer(gpu_meshes.get_state().buffers[0]);
        context->destroy_buffer(gpu_materials.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_groups.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_indices.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_culled_meshlet_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_index_buffer.get_state().buffers[0]);

        for(auto& mesh_manifest : mesh_manifest_entries) {
            if(!mesh_manifest.virtual_geometry_render_info->mesh_buffer.is_empty()) {
                context->destroy_buffer(mesh_manifest.virtual_geometry_render_info->mesh_buffer);
            }
        }

        for(auto& texture_manifest : material_texture_manifest_entries) {
            if(!texture_manifest.image_id.is_empty()) {
                context->destroy_image(texture_manifest.image_id);
            }
        }
    }

    void AssetManager::load_model(LoadManifestInfo& info) {
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }

        auto* mc = info.parent.add_component<ModelComponent>();
        mc->path = info.path;

        u32 const gltf_asset_manifest_index = static_cast<u32>(gltf_asset_manifest_entries.size());
        u32 const texture_manifest_offset = static_cast<u32>(material_texture_manifest_entries.size());
        u32 const material_manifest_offset = static_cast<u32>(material_manifest_entries.size());
        u32 const mesh_manifest_offset = static_cast<u32>(mesh_manifest_entries.size());
        u32 const mesh_group_manifest_offset = static_cast<u32>(mesh_group_manifest_entries.size());

        std::filesystem::path binary_path = info.path;
        binary_path.replace_extension("bmodel");

        std::ifstream file(binary_path, std::ios::binary);
        auto size = std::filesystem::file_size(binary_path);
        std::vector<std::byte> data = {};
        data.resize(size);
        file.read(r_cast<char*>(data.data()), size); 

        std::vector<std::byte> uncompressed_data = {};
        usize uncompressed_data_size = ZSTD_getFrameContentSize(data.data(), data.size());
        uncompressed_data.resize(uncompressed_data_size);
        uncompressed_data_size = ZSTD_decompress(uncompressed_data.data(), uncompressed_data.size(), data.data(), data.size());

        ByteReader byte_reader{ uncompressed_data.data(), uncompressed_data.size() };

        BinaryHeader header = {};
        byte_reader.read(header.name);
        byte_reader.read(header.version);

        std::vector<BinaryTexture> binary_textures = {};
        std::vector<BinaryMaterial> binary_materials = {};
        std::vector<BinaryNode> binary_nodes = {};
        std::vector<BinaryMeshGroup> binary_mesh_groups = {};
        std::vector<ProcessedMeshInfo> binary_meshes = {};

        byte_reader.read(binary_textures);
        byte_reader.read(binary_materials);
        byte_reader.read(binary_nodes);
        byte_reader.read(binary_mesh_groups);
        byte_reader.read(binary_meshes);

        {
            fastgltf::Parser parser{};
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

            fastgltf::GltfDataBuffer data;
            data.loadFromFile(info.path);
            fastgltf::GltfType type = fastgltf::determineGltfFileType(&data);
            fastgltf::Asset asset;
            if (type == fastgltf::GltfType::glTF) {
                asset = std::move(parser.loadGLTF(&data, info.path.parent_path(), gltf_options).get());
            } else if (type == fastgltf::GltfType::GLB) {
                asset = std::move(parser.loadBinaryGLTF(&data, info.path.parent_path(), gltf_options).get());
            }

            gltf_asset_manifest_entries.push_back(GltfAssetManifestEntry {
                .path = info.path,
                .gltf_asset = std::make_unique<fastgltf::Asset>(std::move(asset)),
                .texture_manifest_offset = texture_manifest_offset,
                .material_manifest_offset = material_manifest_offset,
                .mesh_manifest_offset = mesh_manifest_offset,
                .mesh_group_manifest_offset = mesh_group_manifest_offset,
                .parent = info.parent,
                .binary_nodes = binary_nodes,
                .binary_mesh_groups = binary_mesh_groups,
                .binary_meshes = binary_meshes,
            });
        }

        const auto& asset_manifest = gltf_asset_manifest_entries.back();
        const std::unique_ptr<fastgltf::Asset>& asset = asset_manifest.gltf_asset;

        for (u32 i = 0; i < static_cast<u32>(asset->images.size()); ++i) {
            std::vector<TextureManifestEntry::MaterialManifestIndex> indices = {};
            indices.reserve(binary_textures[i].material_indices.size());
            for(const auto& v : binary_textures[i].material_indices) {
                indices.push_back({
                    .albedo = v.albedo,
                    .normal = v.normal,
                    .roughness_metalness = v.roughness_metalness,
                    .emissive = v.emissive,
                    .material_manifest_index = v.material_index + material_manifest_offset,
                });
            }

            material_texture_manifest_entries.push_back(TextureManifestEntry{
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = i,
                .material_manifest_indices = indices, 
                .image_id = {},
                .sampler_id = {}, 
                .name = binary_textures[i].name,
            });
        }

        auto gltf_texture_to_manifest_texture_index = [&](u32 const texture_index) -> std::optional<u32> {
            const bool gltf_texture_has_image_index = asset->textures.at(texture_index).imageIndex.has_value();
            if (!gltf_texture_has_image_index) { return std::nullopt; }
            else { return static_cast<u32>(asset->textures.at(texture_index).imageIndex.value()) + texture_manifest_offset; }
        };

        for (u32 material_index = 0; material_index < static_cast<u32>(asset->materials.size()); material_index++) {
            auto make_texture_info = [texture_manifest_offset](const std::optional<BinaryMaterial::BinaryTextureInfo>& info) -> std::optional<MaterialManifestEntry::TextureInfo> {
                if(!info.has_value()) { return std::nullopt; }
                return std::make_optional(MaterialManifestEntry::TextureInfo {
                    .texture_manifest_index = info->texture_index + texture_manifest_offset,
                    .sampler_index = 0
                });
            };

            const BinaryMaterial& material = binary_materials[material_index];
            material_manifest_entries.push_back(MaterialManifestEntry{
                .albedo_info = make_texture_info(material.albedo_info),
                .normal_info = make_texture_info(material.normal_info),
                .roughness_metalness_info = make_texture_info(material.roughness_metalness_info),
                .emissive_info = make_texture_info(material.emissive_info),
                .metallic_factor = material.metallic_factor,
                .roughness_factor = material.roughness_factor,
                .emissive_factor = material.emissive_factor,
                .alpha_mode = material.alpha_mode,
                .alpha_cutoff = material.alpha_cutoff,
                .double_sided = material.double_sided,
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = material_index,
                .material_manifest_index = material_manifest_offset + material_index,
                .name = material.name.c_str(),
            });
        }

        struct LoadMeshTask : Task {
            struct TaskInfo {
                LoadMeshInfo load_info = {};
                AssetProcessor * asset_processor = {};
                u32 manifest_index = {};
            };

            TaskInfo info = {};
            LoadMeshTask(const TaskInfo& info) : info{info} { chunk_count = 1; }

            virtual void callback(u32, u32) override {
                info.asset_processor->load_gltf_mesh(info.load_info);
            };
        };

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_manifest_indices_offset = s_cast<u32>(mesh_manifest_entries.size());
            mesh_manifest_entries.reserve(mesh_manifest_entries.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                dirty_meshes.push_back(s_cast<u32>(mesh_manifest_entries.size()));
                mesh_manifest_entries.push_back(MeshManifestEntry {
                    .gltf_asset_manifest_index = gltf_asset_manifest_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_primitive_index = primitive_index,
                    .virtual_geometry_render_info = std::nullopt
                });
            }

            dirty_mesh_groups.push_back(s_cast<u32>(mesh_group_manifest_entries.size()));
            mesh_group_manifest_entries.push_back(MeshGroupManifestEntry {
                .mesh_manifest_indices_offset = mesh_manifest_indices_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size()),
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = mesh_index,
                .name = gltf_mesh.name.c_str(),
            });
        }

        std::vector<Entity> node_index_to_entity = {};
        for(u32 node_index = 0; node_index < s_cast<u32>(binary_nodes.size()); node_index++) {
            node_index_to_entity.push_back(scene->create_entity("gltf asset " + std::to_string(gltf_asset_manifest_entries.size()) + " " + std::string{binary_nodes[node_index].name}));
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(binary_nodes.size()); node_index++) {
            const auto& node = binary_nodes[node_index];
            Entity& parent_entity = node_index_to_entity[node_index];
            if(node.mesh_index.has_value()) {
                parent_entity.add_component<GlobalTransformComponent>();
                parent_entity.add_component<LocalTransformComponent>();
                auto* mesh_component = parent_entity.add_component<MeshComponent>();
                mesh_component->mesh_group_index = asset_manifest.mesh_group_manifest_offset + node.mesh_index.value();
                parent_entity.add_component<RenderInfo>();
            }

            glm::vec3 position = {};
            glm::vec3 rotation = {};
            glm::vec3 scale = {};
            math::decompose_transform(node.transform, position, rotation, scale);

            auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
            local_transform->set_position(position);
            local_transform->set_rotation(rotation);
            local_transform->set_scale(scale);

            for(u32 children_index = 0; children_index < node.children.size(); children_index++) {
                Entity& child_entity = node_index_to_entity[children_index];
                child_entity.handle.child_of(parent_entity.handle);
            }
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            Entity& entity = node_index_to_entity[node_index];
            auto parent = entity.handle.parent();
            if(!parent.null()) {
                entity.handle.child_of(info.parent.handle);
            }
        }

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);
            const auto& mesh_group_manifest = mesh_group_manifest_entries[mesh_group_manifest_offset + mesh_index];
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                info.thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                    .load_info = {
                        .asset_path = info.path,
                        .asset = asset.get(),
                        .gltf_mesh_index = mesh_index,
                        .gltf_primitive_index = primitive_index,
                        .material_manifest_offset = material_manifest_offset,
                        .manifest_index = mesh_group_manifest.mesh_manifest_indices_offset + primitive_index,
                        .processed_mesh_info = binary_meshes[binary_mesh_groups[mesh_index].mesh_offset + primitive_index]
                    },
                    .asset_processor = info.asset_processor.get(),
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
                info.asset_processor->load_texture(info.load_info);
            };
        };

        for (u32 gltf_texture_index = 0; gltf_texture_index < static_cast<u32>(asset->images.size()); gltf_texture_index++) {
            auto const texture_manifest_index = gltf_texture_index + asset_manifest.texture_manifest_offset;
            auto const & texture_manifest_entry = material_texture_manifest_entries.at(texture_manifest_index);
            bool used_as_albedo = false;
            for (auto const & material_manifest_index : texture_manifest_entry.material_manifest_indices) {
                used_as_albedo |= material_manifest_index.albedo;
                used_as_albedo |= material_manifest_index.emissive;
            }

            if (!texture_manifest_entry.material_manifest_indices.empty()) {
                info.thread_pool->async_dispatch(
                std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                    .load_info = {
                        .asset_path = asset_manifest.path,
                        .asset = asset_manifest.gltf_asset.get(),
                        .gltf_texture_index = gltf_texture_index,
                        .texture_manifest_index = texture_manifest_index,
                        .load_as_srgb = used_as_albedo,
                        .image_path = asset_manifest.path.parent_path() / binary_textures[gltf_texture_index].file_path,
                    },
                    .asset_processor = info.asset_processor.get(),
                }), TaskPriority::LOW);
            }
        }
    }

    void AssetManager::convert_gltf_to_binary(LoadManifestInfo& info, const std::filesystem::path& output_path) {
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }

        std::unique_ptr<fastgltf::Asset> asset = {};
        {
            fastgltf::Parser parser{};
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

            fastgltf::GltfDataBuffer data;
            data.loadFromFile(info.path);
            fastgltf::GltfType type = fastgltf::determineGltfFileType(&data);
            fastgltf::Asset fastgltf_asset;
            if (type == fastgltf::GltfType::glTF) {
                fastgltf_asset = std::move(parser.loadGLTF(&data, info.path.parent_path(), gltf_options).get());
            } else if (type == fastgltf::GltfType::GLB) {
                fastgltf_asset = std::move(parser.loadBinaryGLTF(&data, info.path.parent_path(), gltf_options).get());
            }

            asset = std::make_unique<fastgltf::Asset>(std::move(fastgltf_asset));
        }

        std::vector<BinaryNode> binary_nodes = {};
        for(const auto& node : asset->nodes) {
            std::vector<u32> children = {};
            for(usize child : node.children) {
                children.push_back(s_cast<u32>(child));
            }

            glm::mat4 transform = {};

            if(const auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform)) {
                glm::quat quat;
                quat.x = trs->rotation[0];
                quat.y = trs->rotation[1];
                quat.z = trs->rotation[2];
                quat.w = trs->rotation[3];
                
                transform = glm::translate(glm::mat4(1.0f), { trs->translation[0], trs->translation[1], trs->translation[2]}) 
                    * glm::toMat4(quat) 
                    * glm::scale(glm::mat4(1.0f), { trs->scale[0], trs->scale[1], trs->scale[2]});
            } else if(const auto* transform_matrix = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
                transform = *r_cast<const glm::mat4*>(transform_matrix);
            }

            binary_nodes.push_back(BinaryNode {
                .mesh_index = node.meshIndex.has_value() ? std::make_optional(s_cast<u32>(node.meshIndex.value())) : std::nullopt,
                .transform = transform,
                .children = children,
                .name = std::string{node.name.c_str()},
            });
        }

        std::vector<BinaryMeshGroup> binary_mesh_groups = {};
        std::vector<ProcessedMeshInfo> binary_meshes = {};

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_offset = s_cast<u32>(binary_meshes.size());
            binary_meshes.reserve(binary_meshes.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                binary_meshes.push_back(AssetProcessor::process_mesh({
                    .asset_path = info.path,
                    .asset = asset.get(),
                    .gltf_mesh_index = mesh_index,
                    .gltf_primitive_index = primitive_index,
                    .material_manifest_offset = 0,
                    .manifest_index = 0,
                }));
            }

            binary_mesh_groups.push_back({
                .mesh_offset = mesh_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size())
            });
        }

        std::random_device random_device;
        std::mt19937_64 engine(random_device());
        std::uniform_int_distribution<uint64_t> uniform_distributation;
        nvtt::Context context(true);

        struct CustomOutputHandler : public nvtt::OutputHandler {
            CustomOutputHandler() = default;
            CustomOutputHandler(const CustomOutputHandler&) = delete;
            CustomOutputHandler& operator=(const CustomOutputHandler&) = delete;
            CustomOutputHandler(CustomOutputHandler&&) = delete;
            CustomOutputHandler& operator=(CustomOutputHandler&&) = delete;
            virtual ~CustomOutputHandler() {}

            virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override {
                data.resize(size);
            }

            virtual bool writeData(const void* ptr, int size) {
                std::memcpy(data.data(), ptr, size);
                return true;
            }

            virtual void endImage() {

            }

            std::vector<std::byte> data = {};
        };

        struct CustomErrorHandler : public nvtt::ErrorHandler {
            void error(nvtt::Error e) override {
                std::println("{}", std::to_string(e));
            }
        };

        std::unique_ptr<CustomErrorHandler> error_handler = std::make_unique<CustomErrorHandler>();

        std::vector<BinaryTexture> binary_textures = {};
        for (u32 i = 0; i < static_cast<u32>(asset->images.size()); ++i) {
            std::vector<std::byte> raw_data = {};
            i32 width = 0;
            i32 height = 0;
            i32 num_channels = 0;
            std::string image_path = info.path.parent_path().string();

            auto get_data = [&](const fastgltf::Buffer& buffer, std::size_t byteOffset, std::size_t byteLength) {
                return std::visit(fastgltf::visitor {
                    [](auto&) -> std::span<const std::byte> {
                        assert(false && "Tried accessing a buffer with no data, likely because no buffers were loaded. Perhaps you forgot to specify the LoadExternalBuffers option?");
                        return {};
                    },
                    [](const fastgltf::sources::Fallback&) -> std::span<const std::byte> {
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

            const auto& image = asset->images[i];

            {
                if(const auto* data = std::get_if<std::monostate>(&image.data)) {
                    std::cout << "std::monostate" << std::endl;
                }
                if(const auto* data = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
                    auto& buffer_view = asset->bufferViews[data->bufferViewIndex];
                    auto content = get_data(asset->buffers[buffer_view.bufferIndex], buffer_view.byteOffset, buffer_view.byteLength);
                    u8* image_data = stbi_load_from_memory(r_cast<const u8*>(content.data()), s_cast<i32>(content.size_bytes()), &width, &height, &num_channels, 4);
                    if(!image_data) { std::cout << "bozo" << std::endl; }
                    raw_data.resize(s_cast<u64>(width * height * 4));
                    std::memcpy(raw_data.data(), image_data, s_cast<u64>(width * height * 4));
                    stbi_image_free(image_data);
                }
                if(const auto* data = std::get_if<fastgltf::sources::URI>(&image.data)) {
                    image_path = info.path.parent_path().string() + '/' + std::string(data->uri.path().begin(), data->uri.path().end());
                    u8* image_data = stbi_load(image_path.c_str(), &width, &height, &num_channels, 4);
                    if(!image_data) { std::cout << "bozo" << std::endl; }
                    raw_data.resize(s_cast<u64>(width * height * 4));
                    std::memcpy(raw_data.data(), image_data, s_cast<u64>(width * height * 4));
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

            std::string file_name = std::to_string(uniform_distributation(engine))+ ".btexture";
            auto binary_image_path = output_path;
            binary_image_path = binary_image_path.parent_path() / file_name;
            
            for(u32 p = 0; p < raw_data.size(); p += 4) {
                std::swap(raw_data[p], raw_data[p+2]);
            }
        

            nvtt::Surface nvtt_image;
            assert(nvtt_image.setImage(nvtt::InputFormat_BGRA_8UB, width, height, 1, raw_data.data()) && "setImage");

            nvtt::CompressionOptions compression_options;
            compression_options.setFormat(nvtt::Format_BC7);
            compression_options.setQuality(nvtt::Quality_Normal);

            nvtt::OutputOptions output_options;
            std::unique_ptr<CustomOutputHandler> output_handler = std::make_unique<CustomOutputHandler>();
            output_options.setOutputHandler(output_handler.get());
            output_options.setErrorHandler(error_handler.get());
            MyBinaryTextureFormat format {
                .width = static_cast<u32>(width),
                .height = static_cast<u32>(height),
                .depth = 1,
                .format = daxa::Format::BC7_UNORM_BLOCK,
                .mipmaps = {}
            };

            const int num_mipmaps = nvtt_image.countMipmaps();
            for(int mip = 0; mip < num_mipmaps; mip++) {
                context.compress(nvtt_image, 0 /* face */, mip, compression_options, output_options);
                format.mipmaps.push_back(output_handler->data);

                if(mip == num_mipmaps - 1) { break; }

                nvtt_image.toLinearFromSrgb();
                nvtt_image.premultiplyAlpha();
                nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                nvtt_image.demultiplyAlpha();
                nvtt_image.toSrgb();
            }


            std::vector<std::byte> compressed_data = {};
            usize file_size = {}; 
            {
                ByteWriter image_writer = {};
                image_writer.write(format);
                usize compressed_data_size = ZSTD_compressBound(image_writer.data.size());
                compressed_data.resize(compressed_data_size);
                file_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), image_writer.data.data(), image_writer.data.size(), 14);
            }


            std::ofstream file(binary_image_path.string().c_str(), std::ios_base::trunc | std::ios_base::binary);
            file.write(r_cast<char const *>(compressed_data.data()), static_cast<std::streamsize>(file_size));
            file.close();

            binary_textures.push_back(BinaryTexture{
                .material_indices = {}, 
                .name = std::string{asset->images[i].name.c_str()},
                .file_path = file_name
            });
        }

        auto gltf_texture_to_texture_index = [&](u32 const texture_index) -> std::optional<u32> {
            const bool gltf_texture_has_image_index = asset->textures.at(texture_index).imageIndex.has_value();
            if (!gltf_texture_has_image_index) { return std::nullopt; }
            else { return static_cast<u32>(asset->textures.at(texture_index).imageIndex.value()); }
        };

        std::vector<BinaryMaterial> binary_materials = {};
        for(u32 material_index = 0; material_index < static_cast<u32>(asset->materials.size()); material_index++) {
            auto const & material = asset->materials.at(material_index);
            u32 const material_manifest_index = material_index;
            bool const has_normal_texture = material.normalTexture.has_value();
            bool const has_albedo_texture = material.pbrData.baseColorTexture.has_value();
            bool const has_roughness_metalness_texture = material.pbrData.metallicRoughnessTexture.has_value();
            bool const has_emissive_texture = material.emissiveTexture.has_value();
            std::optional<BinaryMaterial::BinaryTextureInfo> albedo_texture_info = {};
            std::optional<BinaryMaterial::BinaryTextureInfo> normal_texture_info = {};
            std::optional<BinaryMaterial::BinaryTextureInfo> roughnes_metalness_info = {};
            std::optional<BinaryMaterial::BinaryTextureInfo> emissive_info = {};
            if (has_albedo_texture) {
                u32 const texture_index = static_cast<u32>(material.pbrData.baseColorTexture.value().textureIndex);
                auto const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    albedo_texture_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    binary_textures.at(albedo_texture_info->texture_index).material_indices.push_back({
                        .albedo = true,
                        .material_index = material_manifest_index,
                    });
                }
            }

            if (has_normal_texture) {
                u32 const texture_index = static_cast<u32>(material.normalTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    normal_texture_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    binary_textures.at(normal_texture_info->texture_index).material_indices.push_back({
                        .normal = true,
                        .material_index = material_manifest_index,
                    });
                }
            }

            if (has_roughness_metalness_texture) {
                u32 const texture_index = static_cast<u32>(material.pbrData.metallicRoughnessTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    roughnes_metalness_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    binary_textures.at(roughnes_metalness_info->texture_index).material_indices.push_back({
                        .roughness_metalness = true,
                        .material_index = material_manifest_index,
                    });
                }
            }
            if (has_emissive_texture) {
                u32 const texture_index = static_cast<u32>(material.emissiveTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    emissive_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    binary_textures.at(emissive_info->texture_index).material_indices.push_back({
                        .emissive = true,
                        .material_index = material_manifest_index,
                    });
                }
            }
            binary_materials.push_back(BinaryMaterial{
                .albedo_info = albedo_texture_info,
                .normal_info = normal_texture_info,
                .roughness_metalness_info = roughnes_metalness_info,
                .emissive_info = emissive_info,
                .metallic_factor = material.pbrData.metallicFactor,
                .roughness_factor = material.pbrData.roughnessFactor,
                .emissive_factor = { material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2] },
                .alpha_mode = s_cast<u32>(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided,
                .name = std::string{material.name.c_str()},
            });
        }


        std::vector<std::byte> compressed_data = {};
        usize file_size = {};
        {
            BinaryHeader header = {
                .name = "binary model",
                .version = 0
            };

            ByteWriter byte_writer;
            byte_writer.write(header.name);
            byte_writer.write(header.version);

            byte_writer.write(binary_textures);
            byte_writer.write(binary_materials);
            byte_writer.write(binary_nodes);
            byte_writer.write(binary_mesh_groups);
            byte_writer.write(binary_meshes);

            std::println("writer {}", byte_writer.data.size());
            usize compressed_data_size = ZSTD_compressBound(byte_writer.data.size());
            compressed_data.resize(compressed_data_size);
            file_size = ZSTD_compress(compressed_data.data(), compressed_data_size, byte_writer.data.data(), byte_writer.data.size(), 14);
        }
    
        std::ofstream file(output_path.string().c_str(), std::ios_base::trunc | std::ios_base::binary);
        file.write(r_cast<char const *>(compressed_data.data()), static_cast<std::streamsize>(file_size));
        file.close();
    }

    auto AssetManager::record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList {
        ZoneScoped;
        auto cmd_recorder = context->device.create_command_recorder({ .name = "asset manager update" });

        auto realloc = [&](daxa::TaskBuffer& task_buffer, u32 new_size) {
            daxa::BufferId buffer = task_buffer.get_state().buffers[0];
            auto info = context->device.info_buffer(buffer).value();
            if(info.size < new_size) {
                context->destroy_buffer_deferred(cmd_recorder, buffer);
                std::println("INFO: {} resized from {} bytes to {} bytes", std::string{info.name.c_str().data()}, std::to_string(info.size), std::to_string(new_size));
                u32 old_size = s_cast<u32>(info.size);
                info.size = new_size;
                daxa::BufferId new_buffer = context->create_buffer(info);
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = buffer,
                    .dst_buffer = new_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = old_size,
                });

                task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
            }
        };

        auto realloc_special = [&](daxa::TaskBuffer& task_buffer, u32 new_size, auto lambda) {
            daxa::BufferId buffer = task_buffer.get_state().buffers[0];
            auto info = context->device.info_buffer(buffer).value();
            if(info.size < new_size) {
                context->destroy_buffer_deferred(cmd_recorder, buffer);
                std::println("INFO: {} resized from {} bytes to {} bytes", std::string{info.name.c_str().data()}, std::to_string(info.size), std::to_string(new_size));
                info.size = new_size;
                daxa::BufferId new_buffer = context->create_buffer(info);

                auto data = lambda(new_buffer);

                daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = sizeof(decltype(data)),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = std::string{"staging"} + std::string{info.name.c_str().data()}
                });
                context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
                std::memcpy(context->device.get_host_address(staging_buffer).value(), &data, sizeof(decltype(data)));
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = new_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = sizeof(decltype(data)),
                });

                task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
            }
        };

        for(const auto& mesh_upload_info : info.uploaded_meshes) {
            total_meshlet_count += mesh_upload_info.meshlet_count;
            total_triangle_count += mesh_upload_info.triangle_count;
            total_vertex_count += mesh_upload_info.vertex_count;
        }

        realloc(gpu_meshes, s_cast<u32>(mesh_manifest_entries.size() * sizeof(Mesh)));
        realloc(gpu_materials, s_cast<u32>(material_manifest_entries.size() * sizeof(Material)));
        realloc(gpu_mesh_groups, s_cast<u32>(mesh_group_manifest_entries.size() * sizeof(MeshGroup)));
        realloc(gpu_mesh_indices, s_cast<u32>(mesh_manifest_entries.size() * sizeof(u32)));
        realloc_special(gpu_meshlet_data, total_meshlet_count * sizeof(MeshletData) + sizeof(MeshletsData), [&](const daxa::BufferId& buffer) -> MeshletsData {
            return MeshletsData {
                .count = 0,
                .meshlets = context->device.get_device_address(buffer).value() + sizeof(MeshletsData)
            };
        });
        realloc_special(gpu_culled_meshlet_data, total_meshlet_count * sizeof(MeshletData) + sizeof(MeshletsData), [&](const daxa::BufferId& buffer) -> MeshletsData {
            return MeshletsData {
                .count = 0,
                .meshlets = context->device.get_device_address(buffer).value() + sizeof(MeshletsData)
            };
        });
        realloc_special(gpu_meshlet_index_buffer, total_triangle_count * sizeof(u32) + sizeof(MeshletIndexBuffer), [&](const daxa::BufferId& buffer) -> MeshletIndexBuffer {
            return MeshletIndexBuffer {
                .count = 0,
                .indices = context->device.get_device_address(buffer).value() + sizeof(MeshletIndexBuffer)
            };
        });

        if(!dirty_meshes.empty()) {
            Mesh mesh = {};

            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer meshes"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::memcpy(context->device.get_host_address(staging_buffer).value(), &mesh, sizeof(Mesh));

            for(const u32 mesh_index : dirty_meshes) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_meshes.get_state().buffers[0],
                    .src_offset = 0,
                    .dst_offset = mesh_index * sizeof(Mesh),
                    .size = sizeof(Mesh)
                });
            }
            
            dirty_meshes = {};
        }

        if(!dirty_mesh_groups.empty()) {
            std::vector<MeshGroup> mesh_groups = {};
            std::vector<u32> meshes = {};
            u64 buffer_ptr = context->device.get_device_address(gpu_mesh_indices.get_state().buffers[0]).value();
            mesh_groups.reserve(dirty_mesh_groups.size());
            for(u32 mesh_group_index : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_index];
                
                mesh_groups.push_back(MeshGroup {
                    .mesh_indices = buffer_ptr + mesh_group.mesh_manifest_indices_offset * sizeof(u32),
                    .count = mesh_group.mesh_count
                });
                
                meshes.reserve(meshes.size() + mesh_group.mesh_count);
                for(u32 i = 0; i < mesh_group.mesh_count; i++) {
                    meshes.push_back(mesh_group.mesh_manifest_indices_offset + i);
                }
            }

            usize staging_size = mesh_groups.size() * sizeof(MeshGroup) + meshes.size() * sizeof(u32);
            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = staging_size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer scene data"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::byte* ptr = context->device.get_host_address(staging_buffer).value();

            usize offset = 0; 
            std::memcpy(ptr + offset, mesh_groups.data(), mesh_groups.size() * sizeof(MeshGroup));
            offset += mesh_groups.size() * sizeof(MeshGroup);
            std::memcpy(ptr + offset, meshes.data(), meshes.size() * sizeof(u32));
            offset += meshes.size() * sizeof(u32);

            usize mesh_group_offset = 0;
            usize meshes_offset = mesh_group_offset + mesh_groups.size() * sizeof(MeshGroup);
            for(u32 mesh_group_index : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_index];

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_groups.get_state().buffers[0],
                    .src_offset = mesh_group_offset,
                    .dst_offset = mesh_group_index * sizeof(MeshGroup),
                    .size = sizeof(MeshGroup)
                });

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_indices.get_state().buffers[0],
                    .src_offset = meshes_offset,
                    .dst_offset = mesh_group.mesh_manifest_indices_offset * sizeof(u32),
                    .size = mesh_group.mesh_count * sizeof(u32)
                });

                mesh_group_offset += sizeof(MeshGroup);
                meshes_offset += mesh_group.mesh_count * sizeof(u32);
            }
            dirty_mesh_groups.clear();
        }

        daxa::BufferId material_null_buffer = context->create_buffer({
            .size = static_cast<u32>(sizeof(Material)),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "material null buffer",
        });
        context->destroy_buffer_deferred(cmd_recorder, material_null_buffer);
        {
            Material material {
                .albedo_texture_id = {},
                .albedo_sampler_id = {},
                .normal_texture_id = {},
                .normal_sampler_id = {},
                .roughness_metalness_texture_id = {},
                .roughness_metalness_sampler_id = {},
                .emissive_texture_id = {},
                .emissive_sampler_id = {},
                .metallic_factor = 1.0f,
                .roughness_factor = 1.0f,
                .emissive_factor = { 0.0f, 0.0f, 0.0f },
                .alpha_mode = 0,
                .alpha_cutoff = 0.5f,
                .double_sided = 0u
            };

            std::memcpy(context->device.get_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
        }

        for(auto& mesh_upload_info : info.uploaded_meshes) {
            auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
            auto& gltf_asset_manifest = gltf_asset_manifest_entries[mesh_manifest.gltf_asset_manifest_index];
            u32 material_index = mesh_upload_info.material_manifest_offset + static_cast<u32>(gltf_asset_manifest.gltf_asset->meshes[mesh_manifest.asset_local_mesh_index].primitives[mesh_manifest.asset_local_primitive_index].materialIndex.value());
            auto& material_manifest = material_manifest_entries.at(material_index);

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = material_null_buffer,
                .dst_buffer = gpu_materials.get_state().buffers[0],
                .src_offset = 0,
                .dst_offset = material_manifest.material_manifest_index * sizeof(Material),
                .size = sizeof(Material),
            });

            mesh_manifest.virtual_geometry_render_info = MeshManifestEntry::VirtualGeometryRenderInfo {
                .mesh_buffer = mesh_upload_info.mesh_buffer,
                .material_manifest_index = material_manifest.material_manifest_index,
            };
        }

        if(!info.uploaded_meshes.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = info.uploaded_meshes.size() * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Mesh * staging_ptr = context->device.get_host_address_as<Mesh>(staging_buffer).value();

            for (u32 upload_index = 0; upload_index < info.uploaded_meshes.size(); upload_index++) {
                auto const & upload = info.uploaded_meshes[upload_index];
                std::memcpy(staging_ptr + upload_index, &upload.mesh, sizeof(Mesh));

                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_meshes.get_state().buffers[0],
                    .src_offset = upload_index * sizeof(Mesh),
                    .dst_offset = upload.manifest_index * sizeof(Mesh),
                    .size = sizeof(Mesh),
                });
            }
        }

        std::vector<u32> dirty_material_manifest_indices = {};
        for(auto& texture_upload_info : info.uploaded_textures) {
            auto& texture_manifest = material_texture_manifest_entries.at(texture_upload_info.manifest_index);
            texture_manifest.image_id = texture_upload_info.dst_image;
            texture_manifest.sampler_id = texture_upload_info.sampler;

            for(auto& material_using_texture_info : texture_manifest.material_manifest_indices) {
                MaterialManifestEntry & material_entry = material_manifest_entries.at(material_using_texture_info.material_manifest_index);
                if (material_using_texture_info.albedo) {
                    material_entry.albedo_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.normal) {
                    material_entry.normal_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.roughness_metalness) {
                    material_entry.roughness_metalness_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.emissive) {
                    material_entry.emissive_info->texture_manifest_index = texture_upload_info.manifest_index;
                }

                if (std::find(dirty_material_manifest_indices.begin(), dirty_material_manifest_indices.end(), material_using_texture_info.material_manifest_index) == dirty_material_manifest_indices.end()) {
                    dirty_material_manifest_indices.push_back(material_using_texture_info.material_manifest_index);
                }
            }
        }

        if(!dirty_material_manifest_indices.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = static_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer material manifest",
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Material* ptr = context->device.get_host_address_as<Material>(staging_buffer).value();
            for (u32 dirty_materials_index = 0; dirty_materials_index < dirty_material_manifest_indices.size(); dirty_materials_index++) {
                MaterialManifestEntry& material = material_manifest_entries.at(dirty_material_manifest_indices.at(dirty_materials_index));
                daxa::ImageId albedo_image_id = {};
                daxa::SamplerId albedo_sampler_id = {};
                daxa::ImageId normal_image_id = {};
                daxa::SamplerId normal_sampler_id = {};
                daxa::ImageId roughness_metalness_image_id = {};
                daxa::SamplerId roughness_metalness_sampler_id = {};
                daxa::ImageId emissive_image_id = {};
                daxa::SamplerId emissive_sampler_id = {};

                if (material.albedo_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.albedo_info.value().texture_manifest_index);
                    albedo_image_id = texture_entry.image_id;
                    albedo_sampler_id = texture_entry.sampler_id;
                }
                if (material.normal_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.normal_info.value().texture_manifest_index);
                    normal_image_id = texture_entry.image_id;
                    normal_sampler_id = texture_entry.sampler_id;
                }
                if (material.roughness_metalness_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.roughness_metalness_info.value().texture_manifest_index);
                    roughness_metalness_image_id = texture_entry.image_id;
                    roughness_metalness_sampler_id = texture_entry.sampler_id;
                }
                if (material.emissive_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.emissive_info.value().texture_manifest_index);
                    emissive_image_id = texture_entry.image_id;
                    emissive_sampler_id = texture_entry.sampler_id;
                }

                ptr[dirty_materials_index] = {
                    .albedo_texture_id = albedo_image_id.default_view(),
                    .albedo_sampler_id = albedo_sampler_id,
                    .normal_texture_id = normal_image_id.default_view(),
                    .normal_sampler_id = normal_sampler_id,
                    .roughness_metalness_texture_id = roughness_metalness_image_id.default_view(),
                    .roughness_metalness_sampler_id = roughness_metalness_sampler_id,
                    .emissive_texture_id = emissive_image_id.default_view(),
                    .emissive_sampler_id = emissive_sampler_id,
                    .metallic_factor = material.metallic_factor,
                    .roughness_factor = material.roughness_factor,
                    .emissive_factor = material.emissive_factor,
                    .alpha_mode = material.alpha_mode,
                    .alpha_cutoff = material.alpha_cutoff,
                    .double_sided = s_cast<b32>(material.double_sided)
                };

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_materials.get_state().buffers[0],
                    .src_offset = sizeof(Material) * dirty_materials_index,
                    .dst_offset = material.material_manifest_index * sizeof(Material),
                    .size = sizeof(Material),
                });
            }
        }

        cmd_recorder.pipeline_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ_WRITE,
        });

        return cmd_recorder.complete_current_commands();
    }
}
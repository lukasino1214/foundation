#include "asset_manager.hpp"
#include "graphics/gpu_context.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <dds.hpp>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULE__AUX__IMAGE
#define WUFFS_CONFIG__MODULE__JPEG
#define WUFFS_CONFIG__MODULE__PNG
#include <wuffs-v0.4.c>

#include <meshoptimizer.h>

#include <ecs/entity.hpp>
#include <ecs/scene.hpp>

#include <math/math.hpp>

namespace foundation {
    auto dds_format_to_daxa_format(DXGI_FORMAT format, bool alpha_flag) -> daxa::Format {
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
            // return AssetProcessor::AssetLoadResultCode::ERROR_FAULTY_BUFFER_VIEW;
        }
        fastgltf::sources::URI uri = std::get<fastgltf::sources::URI>(gltf_buffer.data);

        /// NOTE: load the section of the file containing the buffer for the mesh index buffer.
        std::filesystem::path const full_buffer_path = root_path / uri.uri.fspath();
        std::ifstream ifs{full_buffer_path, std::ios::binary};
        if (!ifs)
        {
            // return AssetProcessor::AssetLoadResultCode::ERROR_COULD_NOT_OPEN_GLTF;
        }
        /// NOTE: Only load the relevant part of the file containing the view of the buffer we actually need.
        ifs.seekg(gltf_buffer_view.byteOffset + accesor.byteOffset + uri.fileByteOffset);
        std::vector<u16> raw = {};
        size_t const elem_byte_size = fastgltf::getElementByteSize(accesor.type, accesor.componentType);
        raw.resize((accesor.count * elem_byte_size) / 2);
        /// NOTE: Only load the relevant part of the file containing the view of the buffer we actually need.
        if (!ifs.read(r_cast<char *>(raw.data()), accesor.count * elem_byte_size))
        {
            // return AssetProcessor::AssetLoadResultCode::ERROR_COULD_NOT_READ_BUFFER_IN_GLTF;
        }
        auto buffer_adapter = [&](fastgltf::Asset const & asset, u32 asset_index) -> fastgltf::span<std::byte const>
        {
            /// NOTE:   We only have a ptr to the loaded data to the accessors section of the buffer.
            ///         Fastgltf expects a ptr to the begin of the buffer VIEW, so we just subtract the offsets.
            ///         Fastgltf adds these on in the accessor tool, so in the end it gets the right ptr.
            return fastgltf::span<std::byte const>(reinterpret_cast<std::byte const *>(raw.data()) - accesor.byteOffset, accesor.count * elem_byte_size);
        };

        std::vector<ElemT> ret(accesor.count);
        if constexpr (IS_INDEX_BUFFER)
        {
            /// NOTE: Transform the loaded file section into a 32 bit index buffer.
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
            IScene scene = {};
            scene.name = gltf_scene.name.c_str();

            scene.node_indices.reserve(gltf_scene.nodeIndices.size());
            for (usize value : gltf_scene.nodeIndices) {
                scene.node_indices.push_back(s_cast<u32>(value));
            }

            model_data.scenes.push_back(scene);
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

            glm::mat4 transform = {};
            if(const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
                glm::quat quat;
                quat.x = trs->rotation[0];
                quat.y = trs->rotation[1];
                quat.z = trs->rotation[2];
                quat.w = trs->rotation[3];
                
                transform = glm::translate(glm::mat4(1.0f), { trs->translation[0], trs->translation[1], trs->translation[2]}) 
                    * glm::toMat4(quat) 
                    * glm::scale(glm::mat4(1.0f), { trs->scale[0], trs->scale[1], trs->scale[2]});
            } else if(const auto* transform_matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
                transform = *r_cast<const glm::mat4*>(transform_matrix);
            }

            glm::vec3 position = {};
            glm::vec3 rotation = {};
            glm::vec3 scale = {};
            math::decompose_transform(transform, position, rotation, scale);

            parent_entity.add_component<TransformComponent>();
            parent_entity.set_local_position(position);
            parent_entity.set_local_rotation(rotation);
            parent_entity.set_local_scale(scale);

            for(u32 children_index : node.children) {
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
        for (const fastgltf::Mesh& gltf_mesh : asset->meshes) {
            MeshGroup mesh_group = {};
            mesh_group.name = gltf_mesh.name.c_str();

            mesh_group.meshes.reserve(gltf_mesh.primitives.size());
            for (const fastgltf::Primitive& primitive : gltf_mesh.primitives) {

                Mesh mesh = {};
                mesh.material_index = primitive.materialIndex.has_value() ? std::make_optional(s_cast<u32>(primitive.materialIndex.value())) : std::nullopt;

                std::vector<glm::vec3> positions = {};
                const fastgltf::Attribute* position_attribute_iter = primitive.findAttribute("POSITION");
                if(position_attribute_iter != primitive.attributes.end()) {
                    positions = load_accessor_data_from_file<glm::vec3, false>(file_path.parent_path(), *asset, asset->accessors[position_attribute_iter->accessorIndex]);
                }

                std::vector<glm::vec3> normals = {};
                const fastgltf::Attribute* normal_attribute_iter = primitive.findAttribute("NORMAL");
                if(normal_attribute_iter != primitive.attributes.end()) {
                    normals = load_accessor_data_from_file<glm::vec3, false>(file_path.parent_path(), *asset, asset->accessors[normal_attribute_iter->accessorIndex]);
                }

                std::vector<glm::vec2> uvs = {};
                const fastgltf::Attribute* uv_attribute_iter = primitive.findAttribute("TEXCOORD_0");
                if(uv_attribute_iter != primitive.attributes.end()) {
                    uvs = load_accessor_data_from_file<glm::vec2, false>(file_path.parent_path(), *asset, asset->accessors[uv_attribute_iter->accessorIndex]);
                }

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
                
                std::vector<u32> indices = {};
                if(primitive.indicesAccessor.has_value()) {
                    indices = load_accessor_data_from_file<u32, true>(file_path.parent_path(), *asset, asset->accessors[primitive.indicesAccessor.value()]);
                }
                
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
                mesh.mesh.material_index = s_cast<u32>(primitive.materialIndex.value_or(INVALID_ID));

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
            std::filesystem::path full_path = file_path.parent_path() / uri->uri.fspath();
            if(full_path.extension() != ".dds") {
                fmt::println("Couldnt load image: {}", (file_path.parent_path() / uri->uri.fspath()).string());
                model_data.images.push_back(image);
                continue; 
            }

            dds::Image dds_image = {};
            dds::ReadResult result =dds::readFile(full_path.string(), &dds_image);
            if (result != dds::ReadResult::Success) {
                throw std::runtime_error("huh");
            }

            daxa::Format daxa_format = dds_format_to_daxa_format(dds_image.format, dds_image.supportsAlpha);
        
            usize total_mipmap_size = 0;
            for(const auto& mipmap : dds_image.mipmaps) {
                total_mipmap_size += mipmap.size();
            }

            daxa::BufferId staging_image_buffer = gpu_context->device.create_buffer({
                .size = total_mipmap_size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = fmt::format("staging_texture_{}", image.name),
            });

            std::byte* staging_ptr = gpu_context->device.buffer_host_address(staging_image_buffer).value();
            usize staging_offset = 0;
            for(const auto& mipmap : dds_image.mipmaps) {
                std::memcpy(staging_ptr + staging_offset, mipmap.data(), mipmap.size());
                staging_offset += mipmap.size();
            }

            image.image = gpu_context->device.create_image({
                .format = daxa_format,
                .size = {dds_image.width, dds_image.height, dds_image.depth},
                .mip_level_count = s_cast<u32>(dds_image.mipmaps.size()),
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
                .max_lod = s_cast<f32>(dds_image.mipmaps.size()),
                .enable_unnormalized_coordinates = false,
            });

            cmd_recorder.pipeline_image_barrier(daxa::ImageBarrierInfo {
                .image_id = image.image,
                .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
            });
           
            staging_offset = 0;
            for(u32 mip_level = 0; mip_level < dds_image.mipmaps.size(); ++mip_level) {
                const auto& mipmap = dds_image.mipmaps[mip_level];
                
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
                        std::max(1u, dds_image.width >> mip_level),
                        std::max(1u, dds_image.height >> mip_level),
                        std::max(1u, dds_image.depth >> mip_level),
                    },
                });

                staging_offset += mipmap.size();
            }

            cmd_recorder.destroy_buffer_deferred(staging_image_buffer);
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
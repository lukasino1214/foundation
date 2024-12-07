#include "asset_processor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <meshoptimizer.h>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <shader_library/vertex_compression.inl>
#include <random>
#include <nvtt/nvtt.h>
#include <utils/zstd.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>
#include <utils/file_io.hpp>

namespace foundation {
    AssetProcessor::AssetProcessor(Context* _context) : context{_context}, mesh_upload_mutex{std::make_unique<std::mutex>()} { PROFILE_SCOPE; }
    AssetProcessor::~AssetProcessor() {}

    void AssetProcessor::convert_gltf_to_binary(const std::filesystem::path& input_path, const std::filesystem::path& output_path) {
        if(!std::filesystem::exists(input_path)) {
            throw std::runtime_error("couldnt not find model: " + input_path.string());
        }

        std::unique_ptr<fastgltf::Asset> asset = {};
        {
            fastgltf::Parser parser{};
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

            fastgltf::GltfDataBuffer data;
            data.loadFromFile(input_path);
            fastgltf::GltfType type = fastgltf::determineGltfFileType(&data);
            fastgltf::Asset fastgltf_asset;
            if (type == fastgltf::GltfType::glTF) {
                fastgltf_asset = std::move(parser.loadGLTF(&data, input_path.parent_path(), gltf_options).get());
            } else if (type == fastgltf::GltfType::GLB) {
                fastgltf_asset = std::move(parser.loadBinaryGLTF(&data, input_path.parent_path(), gltf_options).get());
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
        std::vector<BinaryMesh> binary_meshes = {};

        std::random_device random_device;
        std::mt19937_64 engine(random_device());
        std::uniform_int_distribution<uint64_t> uniform_distributation;

        u32 meshlet_count = {};
        u32 triangle_count = {};
        u32 vertex_count = {};

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_offset = s_cast<u32>(binary_meshes.size());
            binary_meshes.reserve(binary_meshes.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                std::vector<std::byte> compressed_data = {};

                {
                    ProcessedMeshInfo processed_mesh_info = AssetProcessor::process_mesh({
                        .asset = asset.get(),
                        .gltf_mesh_index = mesh_index,
                        .gltf_primitive_index = primitive_index,
                    });

                    meshlet_count += s_cast<u32>(processed_mesh_info.meshlets.size());
                    vertex_count += s_cast<u32>(processed_mesh_info.positions.size());
                    for(const auto& meshlet : processed_mesh_info.meshlets) {
                        triangle_count += meshlet.triangle_count;
                    }

                    ByteWriter mesh_writer = {};
                    mesh_writer.write(processed_mesh_info);

                    compressed_data = zstd_compress(mesh_writer.data, 14);
                }

                std::string file_name = std::to_string(uniform_distributation(engine))+ ".bmesh";
                auto binary_mesh_path = output_path;
                binary_mesh_path = binary_mesh_path.parent_path() / file_name;

                while(std::filesystem::exists(binary_mesh_path)) {
                    file_name = std::to_string(uniform_distributation(engine))+ ".bmesh";
                    binary_mesh_path = output_path;
                    binary_mesh_path = binary_mesh_path.parent_path() / file_name;
                }

                write_bytes_to_file(compressed_data, binary_mesh_path);

                binary_meshes.push_back(BinaryMesh {
                    .material_index = gltf_mesh.primitives[primitive_index].materialIndex.has_value() ? std::make_optional(s_cast<u32>(gltf_mesh.primitives[primitive_index].materialIndex.value())) : std::nullopt,
                    .file_path = file_name
                });

                std::println("mesh group: [{} / {}] - mesh: [{} / {}] - done", mesh_index+1, asset->meshes.size(), primitive_index+1, gltf_mesh.primitives.size());
            }

            binary_mesh_groups.push_back({
                .mesh_offset = mesh_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size()),
                .name = gltf_mesh.name.c_str()
            });
        }

        nvtt::Context context(true);

        struct CustomOutputHandler : public nvtt::OutputHandler {
            CustomOutputHandler() = default;
            CustomOutputHandler(const CustomOutputHandler&) = delete;
            CustomOutputHandler& operator=(const CustomOutputHandler&) = delete;
            CustomOutputHandler(CustomOutputHandler&&) = delete;
            CustomOutputHandler& operator=(CustomOutputHandler&&) = delete;
            virtual ~CustomOutputHandler() {}

            void beginImage(i32 size, i32 /*width*/, i32 /*height*/, i32 /*depth*/, i32 /*face*/, i32 /*miplevel*/) override { data.resize(s_cast<usize>(size)); }
            auto writeData(const void* ptr, i32 size) -> bool override { std::memcpy(data.data(), ptr, s_cast<usize>(size)); return true; }
            void endImage() override {}

            std::vector<std::byte> data = {};
        };

        struct CustomErrorHandler : public nvtt::ErrorHandler {
            void error(nvtt::Error e) override {
                std::println("{}", std::to_string(e));
            }
        };

        std::unique_ptr<CustomErrorHandler> error_handler = std::make_unique<CustomErrorHandler>();

        std::vector<BinaryTexture> binary_textures = {};
        for (u32 i = 0; i < s_cast<u32>(asset->images.size()); ++i) {
            binary_textures.push_back(BinaryTexture{
                .material_indices = {}, 
                .name = std::string{asset->images[i].name.c_str()},
                .file_path = {}
            });
        }

        auto gltf_texture_to_texture_index = [&](u32 const texture_index) -> std::optional<u32> {
            const bool gltf_texture_has_image_index = asset->textures.at(texture_index).imageIndex.has_value();
            if (!gltf_texture_has_image_index) { return std::nullopt; }
            else { return s_cast<u32>(asset->textures.at(texture_index).imageIndex.value()); }
        };

        std::vector<BinaryMaterial> binary_materials = {};
        for(u32 material_index = 0; material_index < s_cast<u32>(asset->materials.size()); material_index++) {
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
                u32 const texture_index = s_cast<u32>(material.pbrData.baseColorTexture.value().textureIndex);
                auto const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    albedo_texture_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    binary_textures.at(albedo_texture_info->texture_index).material_indices.push_back({
                        .material_type = MaterialType::GltfAlbedo,
                        .material_index = material_manifest_index,
                    });
                }
            }

            if (has_normal_texture) {
                u32 const texture_index = s_cast<u32>(material.normalTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    normal_texture_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    binary_textures.at(normal_texture_info->texture_index).material_indices.push_back({
                        .material_type = MaterialType::GltfNormal,
                        .material_index = material_manifest_index,
                    });
                }
            }

            if (has_roughness_metalness_texture) {
                u32 const texture_index = s_cast<u32>(material.pbrData.metallicRoughnessTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    roughnes_metalness_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    binary_textures.at(roughnes_metalness_info->texture_index).material_indices.push_back({
                        .material_type = MaterialType::GltfRoughnessMetallic,
                        .material_index = material_manifest_index,
                    });
                }
            }
            if (has_emissive_texture) {
                u32 const texture_index = s_cast<u32>(material.emissiveTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_texture_index(texture_index).has_value();
                if (has_index) {
                    emissive_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = gltf_texture_to_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    binary_textures.at(emissive_info->texture_index).material_indices.push_back({
                        .material_type = MaterialType::GltfEmissive,
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

        for (u32 i = 0; i < s_cast<u32>(asset->images.size()); ++i) {
            std::vector<std::byte> raw_data = {};
            i32 width = 0;
            i32 height = 0;
            i32 num_channels = 0;
            std::string image_path = input_path.parent_path().string();

            const auto& binary_texture = binary_textures[i];
            std::vector<MaterialType> cached_material_types = {};
            MaterialType preferred_material_type = MaterialType::None;
            for(const auto& material_index : binary_texture.material_indices) {
                bool found = false;

                for(MaterialType cached_material_type : cached_material_types) {
                    if(material_index.material_type == cached_material_type) {
                        found = true;
                    } else if((cached_material_type == MaterialType::GltfAlbedo && material_index.material_type == MaterialType::GltfEmissive) ||
                              (cached_material_type == MaterialType::GltfEmissive && material_index.material_type == MaterialType::GltfAlbedo)) {
                        preferred_material_type = MaterialType::GltfAlbedo;
                    } else {
                        std::println("explode first {} different {}", std::to_string(s_cast<u32>(cached_material_type)), std::to_string(s_cast<u32>(material_index.material_type)));
                        std::abort();
                    }
                }

                if(!found) { cached_material_types.push_back(material_index.material_type); }
            }

            if(preferred_material_type == MaterialType::None) {
                if(cached_material_types.size() != 1) {
                    std::println("explode");
                    std::abort();
                } else {
                    preferred_material_type = cached_material_types[0];
                }
            }

            bool create_alpha_mask = false;
            for(const auto& material_index : binary_texture.material_indices) {
                const auto& material = binary_materials[material_index.material_index];
                create_alpha_mask |= material.alpha_mode != 0;
            }

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
                if(const auto* _ = std::get_if<std::monostate>(&image.data)) {
                    std::println("std::monostate");
                }
                if(const auto* data = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
                    auto& buffer_view = asset->bufferViews[data->bufferViewIndex];
                    auto content = get_data(asset->buffers[buffer_view.bufferIndex], buffer_view.byteOffset, buffer_view.byteLength);
                    u8* image_data = stbi_load_from_memory(r_cast<const u8*>(content.data()), s_cast<i32>(content.size_bytes()), &width, &height, &num_channels, 4);
                    if(!image_data) { std::println("bozo"); }
                    raw_data.resize(s_cast<u64>(width * height * 4));
                    std::memcpy(raw_data.data(), image_data, s_cast<u64>(width * height * 4));
                    stbi_image_free(image_data);
                }
                if(const auto* data = std::get_if<fastgltf::sources::URI>(&image.data)) {
                    image_path = input_path.parent_path().string() + '/' + std::string(data->uri.path().begin(), data->uri.path().end());
                    u8* image_data = stbi_load(image_path.c_str(), &width, &height, &num_channels, 4);
                    if(!image_data) { std::println("bozo"); }
                    raw_data.resize(s_cast<u64>(width * height * 4));
                    std::memcpy(raw_data.data(), image_data, s_cast<u64>(width * height * 4));
                    stbi_image_free(image_data);
                }
                if(const auto* _ = std::get_if<fastgltf::sources::Vector>(&image.data)) {
                    std::println("fastgltf::sources::Vector");
                }
                if(const auto* _ = std::get_if<fastgltf::sources::CustomBuffer>(&image.data)) {
                    std::println("fastgltf::sources::CustomBuffer");
                }
                if(const auto* _ = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
                    std::println("fastgltf::sources::ByteView");
                }
            }

            auto create_nvtt_image = [](i32 width, i32 height, std::vector<std::byte>& data) -> nvtt::Surface {
                for(u32 p = 0; p < data.size(); p += 4) {
                    std::swap(data[p], data[p+2]);
                }

                nvtt::Surface nvtt_image;
                nvtt_image.setImage(nvtt::InputFormat_BGRA_8UB, width, height, 1, data.data());
                return nvtt_image;
            };

            struct NVTTSettings {
                nvtt::CompressionOptions compression_options = {};
                std::unique_ptr<CustomOutputHandler> output_handler = {};
                nvtt::OutputOptions output_options = {};
            };

            auto create_nvtt_settings = [&error_handler](NVTTSettings& nvtt_settings, nvtt::Format nvtt_format) {
                nvtt_settings.compression_options.setFormat(nvtt_format);
                nvtt_settings.compression_options.setQuality(nvtt::Quality_Normal);
                nvtt_settings.output_handler = std::make_unique<CustomOutputHandler>();
                nvtt_settings.output_options.setOutputHandler(nvtt_settings.output_handler.get());
                nvtt_settings.output_options.setErrorHandler(error_handler.get());
            };

            auto create_file = [&uniform_distributation, &engine, &output_path](const BinaryTextureFileFormat& texture) -> std::string {
                std::vector<std::byte> compressed_data = {};
                {
                    ByteWriter image_writer = {};
                    image_writer.write(texture);
                    compressed_data = zstd_compress(image_writer.data, 14);
                }

                std::string file_name = std::to_string(uniform_distributation(engine))+ ".btexture";
                auto binary_image_path = output_path;
                binary_image_path = binary_image_path.parent_path() / file_name;

                while(std::filesystem::exists(binary_image_path)) {
                    file_name = std::to_string(uniform_distributation(engine))+ ".btexture";
                    binary_image_path = output_path;
                    binary_image_path = binary_image_path.parent_path() / file_name;
                }

                write_bytes_to_file(compressed_data, binary_image_path);
                return file_name;
            };

            if(preferred_material_type == MaterialType::GltfAlbedo) {
                std::vector<std::byte> albedo = {};
                albedo.resize(raw_data.size());

                for(u32 pixel = 0; pixel < albedo.size(); pixel += 4) {
                    albedo[pixel + 0] = raw_data[pixel + 0];
                    albedo[pixel + 1] = raw_data[pixel + 1];
                    albedo[pixel + 2] = raw_data[pixel + 2];
                    albedo[pixel + 3] = std::byte{255};
                }

                {
                    nvtt::Surface nvtt_image = create_nvtt_image(width, height, albedo);
                    nvtt::Format compressed_format = nvtt::Format_BC1;
                    daxa::Format daxa_format = daxa::Format::BC1_RGB_SRGB_BLOCK;

                    NVTTSettings nvtt_settings = {};
                    create_nvtt_settings(nvtt_settings, compressed_format);

                    BinaryTextureFileFormat texture {
                        .width = s_cast<u32>(width),
                        .height = s_cast<u32>(height),
                        .depth = 1,
                        .format = daxa_format,
                        .mipmaps = {}
                    };

                    const i32 num_mipmaps = nvtt_image.countMipmaps();
                    for(i32 mip = 0; mip < num_mipmaps; mip++) {
                        context.compress(nvtt_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                        texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                        if(mip == num_mipmaps - 1) { break; }

                        nvtt_image.toLinearFromSrgb();
                        nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                        nvtt_image.toSrgb();
                    }

                    binary_textures[i].file_path = create_file(texture);
                    binary_textures[i].resolution = s_cast<u32>(width);
                }

                if(create_alpha_mask) {
                    std::vector<std::byte> alpha_mask = {};
                    alpha_mask.resize(albedo.size());
                    f32 average_alpha = [&]() -> f32 {
                        f32 alpha_sum = 0.0f;
                        u32 alpha_count = 0;
                        for(const auto& material_index : binary_textures[i].material_indices) {
                            const BinaryMaterial& binary_material = binary_materials[material_index.material_index];
                            if(binary_material.alpha_mode == 1) {
                                alpha_sum += binary_material.alpha_cutoff;
                                alpha_count += 1;
                            }
                        }

                        return alpha_sum / s_cast<f32>(alpha_count);
                    }();
                    
                    for(u32 pixel = 0; pixel < albedo.size(); pixel += 4) {
                        alpha_mask[pixel + 0] = raw_data[pixel + 3];
                        alpha_mask[pixel + 1] = std::byte{0};
                        alpha_mask[pixel + 2] = std::byte{0};
                        alpha_mask[pixel + 3] = std::byte{0};
                    }

                    nvtt::Surface nvtt_image = create_nvtt_image(width, height, alpha_mask);
                    nvtt::Format compressed_format = nvtt::Format_BC4;
                    daxa::Format daxa_format = daxa::Format::BC4_UNORM_BLOCK;

                    NVTTSettings nvtt_settings = {};
                    create_nvtt_settings(nvtt_settings, compressed_format);

                    BinaryTextureFileFormat texture {
                        .width = s_cast<u32>(width),
                        .height = s_cast<u32>(height),
                        .depth = 1,
                        .format = daxa_format,
                        .mipmaps = {}
                    };

                    const i32 num_mipmaps = nvtt_image.countMipmaps();
                    const f32 coverage = nvtt_image.alphaTestCoverage(average_alpha, 0);
                    for(i32 mip = 0; mip < num_mipmaps; mip++) {
                        context.compress(nvtt_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                        texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                        if(mip == num_mipmaps - 1) { break; }

                        nvtt_image.scaleAlphaToCoverage(coverage, average_alpha);
                        nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Kaiser);
                    }

                    std::string file_name = create_file(texture);

                    const BinaryTexture& albedo_texture = binary_textures[i];
                    u32 alpha_mask_texture_index = s_cast<u32>(binary_textures.size());
                    BinaryTexture alpha_mask_texture = BinaryTexture {
                        .resolution = s_cast<u32>(width),
                        .material_indices = {},
                        .name = {},
                        .file_path = file_name,
                    };

                    for(const auto& material_index : albedo_texture.material_indices) {
                        auto& material = binary_materials[material_index.material_index];
                        
                        if(material.alpha_mode != 0) {
                            material.alpha_mask_info = BinaryMaterial::BinaryTextureInfo {
                                .texture_index = alpha_mask_texture_index,
                                .sampler_index = 0
                            };

                            alpha_mask_texture.material_indices.push_back(BinaryTexture::BinaryMaterialIndex{
                                .material_type = MaterialType::CompressedAlphaMask,
                                .material_index = material_index.material_index
                            });
                        }
                    }

                    binary_textures.push_back(alpha_mask_texture);
                }
            } else if(preferred_material_type == MaterialType::GltfNormal) {
                nvtt::Surface nvtt_image = create_nvtt_image(width, height, raw_data);
                nvtt::Format compressed_format = nvtt::Format_BC5;
                daxa::Format daxa_format = daxa::Format::BC5_UNORM_BLOCK;

                NVTTSettings nvtt_settings = {};
                create_nvtt_settings(nvtt_settings, compressed_format);

                BinaryTextureFileFormat texture {
                    .width = s_cast<u32>(width),
                    .height = s_cast<u32>(height),
                    .depth = 1,
                    .format = daxa_format,
                    .mipmaps = {}
                };

                const i32 num_mipmaps = nvtt_image.countMipmaps();
                for(i32 mip = 0; mip < num_mipmaps; mip++) {
                    nvtt_image.normalizeNormalMap();
                    nvtt::Surface temp = nvtt_image;
                    temp.transformNormals(nvtt::NormalTransform_Orthographic);
                    context.compress(temp, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                    texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                    if(mip == num_mipmaps - 1) { break; }

                    nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                }

                binary_textures[i].file_path = create_file(texture);
                binary_textures[i].resolution = s_cast<u32>(width);
            } else if(preferred_material_type == MaterialType::GltfEmissive) {
                nvtt::Surface nvtt_image = create_nvtt_image(width, height, raw_data);
                nvtt::Format compressed_format = nvtt::Format_BC1;
                daxa::Format daxa_format = daxa::Format::BC1_RGB_SRGB_BLOCK;

                NVTTSettings nvtt_settings = {};
                create_nvtt_settings(nvtt_settings, compressed_format);

                BinaryTextureFileFormat texture {
                    .width = s_cast<u32>(width),
                    .height = s_cast<u32>(height),
                    .depth = 1,
                    .format = daxa_format,
                    .mipmaps = {}
                };

                const i32 num_mipmaps = nvtt_image.countMipmaps();
                for(i32 mip = 0; mip < num_mipmaps; mip++) {
                    context.compress(nvtt_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                    texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                    if(mip == num_mipmaps - 1) { break; }

                    nvtt_image.toLinearFromSrgb();
                    nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                    nvtt_image.toSrgb();
                }

                binary_textures[i].file_path = create_file(texture);
                binary_textures[i].resolution = s_cast<u32>(width);
            } else if(preferred_material_type == MaterialType::GltfRoughnessMetallic) {
                std::vector<std::byte> roughness = {};
                roughness.resize(raw_data.size());

                std::vector<std::byte> metalness = {};
                metalness.resize(raw_data.size());

                for(u32 pixel = 0; pixel < raw_data.size(); pixel += 4) {
                    roughness[pixel + 0] = raw_data[pixel + 1];
                    roughness[pixel + 1] = std::byte{0};
                    roughness[pixel + 2] = std::byte{0};
                    roughness[pixel + 3] = std::byte{0};

                    metalness[pixel + 0] = raw_data[pixel + 2];
                    metalness[pixel + 1] = std::byte{0};
                    metalness[pixel + 2] = std::byte{0};
                    metalness[pixel + 3] = std::byte{0};
                }

                nvtt::Surface nvtt_roughness_image = create_nvtt_image(width, height, roughness);
                nvtt::Surface nvtt_metalness_image = create_nvtt_image(width, height, metalness);
                nvtt::Format compressed_format = nvtt::Format_BC4;
                daxa::Format daxa_format = daxa::Format::BC4_UNORM_BLOCK;

                NVTTSettings nvtt_settings = {};
                create_nvtt_settings(nvtt_settings, compressed_format);

                BinaryTextureFileFormat roughness_texture {
                    .width = s_cast<u32>(width),
                    .height = s_cast<u32>(height),
                    .depth = 1,
                    .format = daxa_format,
                    .mipmaps = {}
                };

                BinaryTextureFileFormat metalness_texture {
                    .width = s_cast<u32>(width),
                    .height = s_cast<u32>(height),
                    .depth = 1,
                    .format = daxa_format,
                    .mipmaps = {}
                };

                const i32 num_mipmaps = nvtt_roughness_image.countMipmaps();
                for(i32 mip = 0; mip < num_mipmaps; mip++) {
                    context.compress(nvtt_roughness_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                    roughness_texture.mipmaps.push_back(nvtt_settings.output_handler->data);
                    context.compress(nvtt_metalness_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                    metalness_texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                    if(mip == num_mipmaps - 1) { break; }

                    nvtt_roughness_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                    nvtt_metalness_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                }

                std::string roughness_file_path = create_file(roughness_texture);
                std::string metalness_file_path = create_file(metalness_texture);

                // convert to roughness
                BinaryTexture& roughness_metallic_texture = binary_textures[i];
                roughness_metallic_texture.file_path = roughness_file_path;
                roughness_metallic_texture.resolution = s_cast<u32>(width);
                u32 metalness_texture_index = s_cast<u32>(binary_textures.size());
                BinaryTexture binary_metalness_texture = BinaryTexture {
                    .resolution = s_cast<u32>(width),
                    .material_indices = {},
                    .name = {},
                    .file_path = metalness_file_path,
                };

                for(auto& material_index : roughness_metallic_texture.material_indices) {
                    material_index.material_type = MaterialType::CompressedRoughness;
                    binary_metalness_texture.material_indices.push_back(BinaryTexture::BinaryMaterialIndex {
                        .material_type = MaterialType::CompressedMetalness,
                        .material_index = material_index.material_index
                    });

                    BinaryMaterial& binary_material = binary_materials[material_index.material_index];
                    binary_material.roughness_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = binary_material.roughness_metalness_info->texture_index,
                        .sampler_index = 0
                    };
                    binary_material.metalness_info = BinaryMaterial::BinaryTextureInfo {
                        .texture_index = metalness_texture_index,
                        .sampler_index = 0
                    };

                    binary_material.roughness_metalness_info = std::nullopt;
                }

                binary_textures.push_back(binary_metalness_texture);
            } else {
                nvtt::Surface nvtt_image = create_nvtt_image(width, height, raw_data);
                nvtt::Format compressed_format = nvtt::Format_BC7;
                daxa::Format daxa_format = (preferred_material_type == MaterialType::GltfAlbedo || preferred_material_type == MaterialType::GltfEmissive) ? daxa::Format::BC7_SRGB_BLOCK : daxa::Format::BC7_UNORM_BLOCK;

                NVTTSettings nvtt_settings = {};
                create_nvtt_settings(nvtt_settings, compressed_format);

                BinaryTextureFileFormat texture {
                    .width = s_cast<u32>(width),
                    .height = s_cast<u32>(height),
                    .depth = 1,
                    .format = daxa_format,
                    .mipmaps = {}
                };

                const i32 num_mipmaps = nvtt_image.countMipmaps();
                for(i32 mip = 0; mip < num_mipmaps; mip++) {
                    context.compress(nvtt_image, 0, mip, nvtt_settings.compression_options, nvtt_settings.output_options);
                    texture.mipmaps.push_back(nvtt_settings.output_handler->data);

                    if(mip == num_mipmaps - 1) { break; }

                    nvtt_image.toLinearFromSrgb();
                    nvtt_image.premultiplyAlpha();
                    nvtt_image.buildNextMipmap(nvtt::MipmapFilter_Box);
                    nvtt_image.demultiplyAlpha();
                    nvtt_image.toSrgb();
                }

                binary_textures[i].file_path = create_file(texture);
            }

            std::println("[{} / {}] - image with path: {} - done", i+1, asset->images.size(), image_path);
        }

        std::vector<std::byte> compressed_data = {};
        {
            ByteWriter byte_writer;
            byte_writer.write(BinaryModelHeader {
                .name = "binary model",
                .version = 0,
                .meshlet_count = meshlet_count,
                .triangle_count = triangle_count,
                .vertex_count = vertex_count,
            });

            byte_writer.write(binary_textures);
            byte_writer.write(binary_materials);
            byte_writer.write(binary_nodes);
            byte_writer.write(binary_mesh_groups);
            byte_writer.write(binary_meshes);

            std::println("writer {}", byte_writer.data.size());
            compressed_data = zstd_compress(byte_writer.data, 14);
        }
    
        write_bytes_to_file(compressed_data, output_path);
    }

    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto load_data(fastgltf::Asset& asset, fastgltf::Accessor& accessor) {
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

    auto AssetProcessor::process_mesh(const ProcessMeshInfo &info) -> ProcessedMeshInfo {
        fastgltf::Asset& gltf_asset = *info.asset;
        fastgltf::Mesh& gltf_mesh = info.asset->meshes[info.gltf_mesh_index];
        fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[info.gltf_primitive_index];

        std::vector<glm::vec3> vert_positions = {};
        auto* position_attribute_iter = gltf_primitive.findAttribute("POSITION");
        if(position_attribute_iter != gltf_primitive.attributes.end()) {
            vert_positions = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[position_attribute_iter->second]);
        }

        auto fill = [](auto& vec, u32 required_size = 0) {
            if(vec.empty()) {
                vec.resize(required_size);
                for(u32 i = 0; i < required_size; i++) {
                    vec[i] = {};
                }
            }
        };

        u32 vertex_count = s_cast<u32>(vert_positions.size());

        std::vector<u32> packed_normals = {};
        {
            std::vector<glm::vec3> vert_normals = {};
            auto* normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
            if(normal_attribute_iter != gltf_primitive.attributes.end()) {
                vert_normals = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[normal_attribute_iter->second]);
            } else {
                fill(vert_normals, vertex_count);
            }

            packed_normals.reserve(packed_normals.size());
            for(const f32vec3& normal : vert_normals) {
                packed_normals.push_back(encode_normal(normal));
            }
        }

        std::vector<u32> packed_uvs = {};
        {
            std::vector<glm::vec2> vert_uvs = {};
            auto* uvs_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
            if(uvs_attribute_iter != gltf_primitive.attributes.end()) {
                vert_uvs = load_data<glm::vec2, false>(gltf_asset, gltf_asset.accessors[uvs_attribute_iter->second]);
            } else {
                fill(vert_uvs, vertex_count);
            }

            packed_uvs.reserve(vert_uvs.size());
            for(const f32vec2& uv : vert_uvs) {
                packed_uvs.push_back(encode_uv(uv));
            }
        }

        std::vector<u32> indices = {};
        indices = load_data<u32, true>(gltf_asset, gltf_asset.accessors[gltf_primitive.indicesAccessor.value()]);

        constexpr usize MAX_VERTICES = MAX_VERTICES_PER_MESHLET;
        constexpr usize MAX_TRIANGLES = MAX_TRIANGLES_PER_MESHLET;
        constexpr f32 CONE_WEIGHT = 1.0f;

        {
            std::vector<u32> optimized_indices(indices.size());
            meshopt_optimizeVertexCache(optimized_indices.data(), indices.data(), indices.size(), vertex_count);
            indices = std::move(optimized_indices);
        }

        size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), MAX_VERTICES, MAX_TRIANGLES);
        std::vector<Meshlet> meshlets(max_meshlets);
        std::vector<u32> meshlet_indirect_vertices(max_meshlets * MAX_VERTICES);
        std::vector<u8> meshlet_micro_indices(max_meshlets * MAX_TRIANGLES * 3);

        size_t meshlet_count = meshopt_buildMeshlets(
            r_cast<meshopt_Meshlet*>(meshlets.data()),
            meshlet_indirect_vertices.data(),
            meshlet_micro_indices.data(),
            indices.data(),
            indices.size(),
            r_cast<const f32*>(vert_positions.data()),
            s_cast<usize>(vertex_count),
            sizeof(glm::vec3),
            MAX_VERTICES,
            MAX_TRIANGLES,
            CONE_WEIGHT
        );

        std::vector<BoundingSphere> meshlet_bounds(meshlet_count);
        std::vector<AABB> meshlet_aabbs(meshlet_count);
        f32vec3 mesh_aabb_max = glm::vec3{std::numeric_limits<f32>::lowest()};
        f32vec3 mesh_aabb_min = glm::vec3{std::numeric_limits<f32>::max()};
        for(size_t meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
            meshopt_Bounds raw_bounds = meshopt_computeMeshletBounds(
                &meshlet_indirect_vertices[meshlets[meshlet_index].indirect_vertex_offset],
                &meshlet_micro_indices[meshlets[meshlet_index].micro_indices_offset],
                meshlets[meshlet_index].triangle_count,
                r_cast<f32 *>(vert_positions.data()),
                s_cast<usize>(vertex_count),
                sizeof(glm::vec3));
            meshlet_bounds[meshlet_index].center.x = raw_bounds.center[0];
            meshlet_bounds[meshlet_index].center.y = raw_bounds.center[1];
            meshlet_bounds[meshlet_index].center.z = raw_bounds.center[2];
            meshlet_bounds[meshlet_index].radius = raw_bounds.radius;

            glm::vec3 min_pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].indirect_vertex_offset]];
            glm::vec3 max_pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].indirect_vertex_offset]];


            for (u32 i = 0; i < meshlets[meshlet_index].vertex_count; ++i) {
                glm::vec3 pos = vert_positions[meshlet_indirect_vertices[meshlets[meshlet_index].indirect_vertex_offset + i]];
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }

            mesh_aabb_min = glm::min(mesh_aabb_min, min_pos);
            mesh_aabb_max = glm::max(mesh_aabb_max, max_pos);

            meshlet_aabbs[meshlet_index].center = (max_pos + min_pos) * 0.5f;
            meshlet_aabbs[meshlet_index].extent =  (max_pos - min_pos) * 0.5f;
        }

        AABB mesh_aabb = {
            .center = (mesh_aabb_max + mesh_aabb_min) * 0.5f,
            .extent =  (mesh_aabb_max - mesh_aabb_min) * 0.5f,
        };

        const Meshlet& last = meshlets[meshlet_count - 1];
        meshlet_indirect_vertices.resize(last.indirect_vertex_offset + last.vertex_count);
        meshlet_micro_indices.resize(last.micro_indices_offset + ((last.triangle_count * 3u + 3u) & ~3u));
        meshlets.resize(meshlet_count);

        return {
            .mesh_aabb = mesh_aabb,
            .positions = vert_positions,
            .normals = packed_normals,
            .uvs = packed_uvs,
            .meshlets = meshlets,
            .bounding_spheres = meshlet_bounds,
            .aabbs = meshlet_aabbs,
            .micro_indices = meshlet_micro_indices,
            .indirect_vertices = meshlet_indirect_vertices,
            .primitive_indices = indices
        };
    }

    void AssetProcessor::load_gltf_mesh(const LoadMeshInfo& info) {
        PROFILE_SCOPE;

        Mesh mesh = info.old_mesh;
        mesh.mesh_buffer = {};
        daxa::BufferId mesh_buffer = {};
        daxa::BufferId staging_mesh_buffer = {};

        if(!context->device.is_buffer_id_valid(std::bit_cast<daxa::BufferId>(info.old_mesh.mesh_buffer))) {

            std::vector<std::byte> uncompressed_data = {};
            {
                std::vector<byte> data = {};
                {
                    PROFILE_ZONE_NAMED(reading_from_disk_and_uncompressing);
                    data = read_file_to_bytes(info.file_path);
                }
                {
                    PROFILE_ZONE_NAMED(decompressing);
                    uncompressed_data = zstd_decompress(data);
                }
            }
            ProcessedMeshInfo processed_info = {};
            {
                PROFILE_ZONE_NAMED(serializing);
                ByteReader reader(uncompressed_data.data(), uncompressed_data.size());
                reader.read(processed_info);
                uncompressed_data.clear();
            }

            {
                PROFILE_ZONE_NAMED(creating_buffer);
                const u64 total_mesh_buffer_size =
                    sizeof(Meshlet) * processed_info.meshlets.size() +
                    sizeof(BoundingSphere) * processed_info.meshlets.size() +
                    sizeof(AABB) * processed_info.meshlets.size() +
                    sizeof(u8) * processed_info.micro_indices.size() +
                    sizeof(u32) * processed_info.indirect_vertices.size() +
                    sizeof(u32) * processed_info.primitive_indices.size() +
                    sizeof(f32vec3) * processed_info.positions.size() +
                    sizeof(u32) * processed_info.normals.size() +
                    sizeof(u32) * processed_info.uvs.size();

                mesh.aabb = processed_info.mesh_aabb; 

                staging_mesh_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = s_cast<daxa::usize>(total_mesh_buffer_size),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "mesh buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.mesh_group_index) + " primitive " + std::to_string(info.mesh_index) + " staging",
                });
                
                mesh_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = s_cast<daxa::usize>(total_mesh_buffer_size),
                    .name = "mesh buffer: " + info.asset_path.filename().string() + " mesh " + std::to_string(info.mesh_group_index) + " primitive " + std::to_string(info.mesh_index)
                });
            }

            {
                PROFILE_ZONE_NAMED(writing_into_buffer);
                daxa::DeviceAddress mesh_bda = context->device.buffer_device_address(std::bit_cast<daxa::BufferId>(mesh_buffer)).value();

                std::byte* staging_ptr = context->device.buffer_host_address(staging_mesh_buffer).value();
                usize accumulated_offset = 0;

                auto memcpy_data = [&](daxa::DeviceAddress& bda, const auto& vec){
                    bda = mesh_bda + accumulated_offset;
                    std::memcpy(staging_ptr + accumulated_offset, vec.data(), vec.size() * sizeof(vec[0]));
                    accumulated_offset += vec.size() * sizeof(vec[0]);
                };

                memcpy_data(mesh.meshlets, processed_info.meshlets);
                memcpy_data(mesh.meshlet_bounds, processed_info.bounding_spheres);
                memcpy_data(mesh.meshlet_aabbs, processed_info.aabbs);
                memcpy_data(mesh.micro_indices, processed_info.micro_indices);
                memcpy_data(mesh.indirect_vertices, processed_info.indirect_vertices);
                memcpy_data(mesh.primitive_indices, processed_info.primitive_indices);
                memcpy_data(mesh.vertex_positions, processed_info.positions);
                memcpy_data(mesh.vertex_normals, processed_info.normals);
                memcpy_data(mesh.vertex_uvs, processed_info.uvs);
                
                mesh.mesh_buffer = mesh_buffer;
                mesh.material_index = info.material_manifest_offset + info.asset->meshes[info.asset->mesh_groups[info.mesh_group_index].mesh_offset + info.mesh_index].material_index.value();
                mesh.meshlet_count = s_cast<u32>(processed_info.meshlets.size());
                mesh.vertex_count = s_cast<u32>(processed_info.positions.size());
            }
        }

        {
            PROFILE_ZONE_NAMED(adding_to_queue);
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            mesh_upload_queue.push_back(MeshUploadInfo {
                .staging_mesh_buffer = staging_mesh_buffer,
                .mesh_buffer = mesh_buffer,
                .old_buffer = std::bit_cast<daxa::BufferId>(info.old_mesh.mesh_buffer),
                .mesh = mesh,
                .manifest_index = info.manifest_index,
                .material_manifest_offset = info.material_manifest_offset,
            });
        }
    }

    void AssetProcessor::load_texture(const LoadTextureInfo& info) {
        PROFILE_SCOPE;

        BinaryTextureFileFormat texture = {};
        u32 mip_levels = {};
        daxa::ImageId daxa_image = {};
        daxa::SamplerId daxa_sampler = {};
        daxa::ImageInfo image_info = {};
        std::vector<TextureOffsets> offsets = {};
        daxa::BufferId staging_buffer = {};

        bool read_file = !context->device.is_image_id_valid(info.old_image);
        if(read_file == false) {
            image_info = context->device.image_info(info.old_image).value();
            if(image_info.size.x < info.requested_resolution || image_info.size.y < info.requested_resolution) { read_file = true; }
        }

        if(read_file) {
            {
                std::vector<std::byte> uncompressed_data = {};
                std::vector<byte> data = read_file_to_bytes(info.image_path);
                uncompressed_data = zstd_decompress(data);
                ByteReader reader(uncompressed_data.data(), uncompressed_data.size());
                reader.read(texture);
            }

            u32 width = info.requested_resolution != 0 ? std:: min(texture.width, info.requested_resolution) : texture.width;
            u32 height = info.requested_resolution != 0 ? std:: min(texture.height, info.requested_resolution) : texture.height;
            mip_levels = s_cast<u32>(std::floor(std::log2(width))) + 1;
            daxa_image = context->create_image(daxa::ImageInfo {
                .dimensions = 2,
                .format = texture.format,
                .size = {width, height, 1},
                .mip_level_count = mip_levels,
                .array_layer_count = 1,
                .sample_count = 1,
                .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::TRANSFER_SRC,
                .name = "image: " + info.image_path.string() + " " + std::to_string(info.texture_index),
            });

            u32 _size = {};

            std::array<i32, 3> mip_size = {
                s_cast<i32>(texture.width),
                s_cast<i32>(texture.height),
                s_cast<i32>(1),
            };

            for(const auto& mipmap : texture.mipmaps) {
                offsets.push_back({
                    .width = s_cast<u32>(mip_size[0]),
                    .height = s_cast<u32>(mip_size[1]),
                    .size = s_cast<u32>(mipmap.size()),
                    .offset = _size
                });
                _size += mipmap.size();
                mip_size = {
                    std::max<i32>(1, mip_size[0] / 2),
                    std::max<i32>(1, mip_size[1] / 2),
                    std::max<i32>(1, mip_size[2] / 2),
                };
            }

            staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = _size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer: " + info.image_path.string() + " " + std::to_string(info.texture_index)
            }); 

            for(u32 i = 0; i < offsets.size(); i++) {
                std::memcpy(context->device.buffer_host_address(staging_buffer).value() + offsets[i].offset, texture.mipmaps[i].data(), s_cast<u64>(offsets[i].size));
            }
        } else {
            mip_levels = static_cast<u32>(std::floor(std::log2(info.requested_resolution))) + 1;
            daxa_image = context->create_image(daxa::ImageInfo {
                .dimensions = 2,
                .format = image_info.format,
                .size = { info.requested_resolution, info.requested_resolution, 1},
                .mip_level_count = mip_levels,
                .array_layer_count = 1,
                .sample_count = 1,
                .usage = image_info.usage,
                .name = image_info.name,
            });
        }

        daxa_sampler = context->get_sampler({
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
            .max_lod = s_cast<f32>(mip_levels),
            .enable_unnormalized_coordinates = false,
        });

        std::lock_guard<std::mutex> lock{*texture_upload_mutex};
        texture_upload_queue.push_back(TextureUploadInfo{
            .offsets = offsets,
            .staging_buffer = staging_buffer,
            .dst_image = daxa_image,
            .old_image = info.old_image,
            .sampler = daxa_sampler,
            .manifest_index = info.texture_manifest_index,
        });
    }

    auto AssetProcessor::record_gpu_load_processing_commands() -> RecordCommands {
        PROFILE_SCOPE;
        RecordCommands ret = {};
        {
            PROFILE_SCOPE_NAMED(getting_meshes);
            std::lock_guard<std::mutex> lock{*mesh_upload_mutex};
            ret.uploaded_meshes = std::move(mesh_upload_queue);
            mesh_upload_queue = {};
        }

        auto cmd_recorder = context->device.create_command_recorder(daxa::CommandRecorderInfo { .name = "asset processor upload" });

        {
            PROFILE_SCOPE_NAMED(mesh_upload_info_);
            for(MeshUploadInfo& mesh_upload_info : ret.uploaded_meshes) {
                if(context->device.is_buffer_id_valid(std::bit_cast<daxa::BufferId>(mesh_upload_info.mesh.mesh_buffer))) {
                    context->destroy_buffer_deferred(cmd_recorder, mesh_upload_info.staging_mesh_buffer);

                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = mesh_upload_info.staging_mesh_buffer,
                        .dst_buffer = mesh_upload_info.mesh_buffer,
                        .src_offset = 0,
                        .dst_offset = 0,
                        .size = context->device.buffer_info(mesh_upload_info.mesh_buffer).value().size
                    });
                } else {
                    context->destroy_buffer_deferred(cmd_recorder, mesh_upload_info.old_buffer);
                }
            }
        }

        {
            PROFILE_SCOPE_NAMED(getting_textures);
            std::lock_guard<std::mutex> lock{*texture_upload_mutex};
            ret.uploaded_textures = std::move(texture_upload_queue);
            texture_upload_queue = {};
        }

        {
            PROFILE_SCOPE_NAMED(texture_upload_info_);
            for(auto& texture_upload_info : ret.uploaded_textures) {
                if(!texture_upload_info.offsets.empty()) {
                    auto image_info = context->device.image_info(texture_upload_info.dst_image).value();

                    std::array<i32, 3> mip_size = {
                        s_cast<i32>(image_info.size.x),
                        s_cast<i32>(image_info.size.y),
                        s_cast<i32>(image_info.size.z),
                    };

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

                    for(u32 old_image_i = 0, image_i = 0; old_image_i < texture_upload_info.offsets.size(); old_image_i++) {
                        const auto& offset = texture_upload_info.offsets[old_image_i];
                        if(offset.width == s_cast<u32>(mip_size[0]) && offset.height == s_cast<u32>(mip_size[1])) {
                            cmd_recorder.copy_buffer_to_image({
                                .buffer = texture_upload_info.staging_buffer,
                                .buffer_offset = offset.offset,
                                .image = texture_upload_info.dst_image,
                                .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                                .image_slice = {
                                    .mip_level = image_i,
                                    .base_array_layer = 0,
                                    .layer_count = 1,
                                },
                                .image_offset = { 0, 0, 0 },
                                .image_extent = { s_cast<u32>(mip_size[0]), s_cast<u32>(mip_size[1]), s_cast<u32>(mip_size[2]) }
                            });
                            mip_size = {
                                std::max<i32>(1, mip_size[0] / 2),
                                std::max<i32>(1, mip_size[1] / 2),
                                std::max<i32>(1, mip_size[2] / 2),
                            };
                            image_i++;
                        }
                    }

                    cmd_recorder.pipeline_barrier_image_transition({
                        .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                        .dst_access = daxa::AccessConsts::READ_WRITE,
                        .src_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                        .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = image_info.mip_level_count,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_id = texture_upload_info.dst_image,
                    });
                } else {
                    const daxa::ImageInfo old_image_info = context->device.image_info(texture_upload_info.old_image).value();
                    const daxa::ImageInfo image_info = context->device.image_info(texture_upload_info.dst_image).value();

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

                    cmd_recorder.pipeline_barrier_image_transition({
                        .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                        .dst_access = daxa::AccessConsts::READ_WRITE,
                        .src_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                        .dst_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = old_image_info.mip_level_count,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_id = texture_upload_info.old_image,
                    });

                    std::array<i32, 3> mip_size = {
                        s_cast<i32>(image_info.size.x),
                        s_cast<i32>(image_info.size.y),
                        s_cast<i32>(image_info.size.z),
                    };

                    std::array<i32, 3> old_mip_size = {
                        s_cast<i32>(old_image_info.size.x),
                        s_cast<i32>(old_image_info.size.y),
                        s_cast<i32>(old_image_info.size.z),
                    };

                    for(u32 old_image_i = 0, image_i = 0; old_image_i < old_image_info.mip_level_count; old_image_i++) {
                        if(old_mip_size[0] == mip_size[0] && old_mip_size[1] == mip_size[1] && old_mip_size[1] == mip_size[1]) {
                            cmd_recorder.copy_image_to_image(daxa::ImageCopyInfo {
                                .src_image = texture_upload_info.old_image,
                                .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                                .dst_image = texture_upload_info.dst_image,
                                .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                                .src_slice = {
                                    .mip_level = old_image_i,
                                    .base_array_layer = 0,
                                    .layer_count = 1,
                                },
                                .dst_slice = {
                                    .mip_level = image_i,
                                    .base_array_layer = 0,
                                    .layer_count = 1,
                                },
                                .extent = { s_cast<u32>(mip_size[0]), s_cast<u32>(mip_size[1]), s_cast<u32>(mip_size[2]) }
                            });

                            mip_size = {
                                std::max<i32>(1, mip_size[0] / 2),
                                std::max<i32>(1, mip_size[1] / 2),
                                std::max<i32>(1, mip_size[2] / 2),
                            };
                            image_i++;
                        }
                        
                        old_mip_size = {
                            std::max<i32>(1, old_mip_size[0] / 2),
                            std::max<i32>(1, old_mip_size[1] / 2),
                            std::max<i32>(1, old_mip_size[2] / 2),
                        };
                    }

                    cmd_recorder.pipeline_barrier_image_transition({
                        .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                        .dst_access = daxa::AccessConsts::READ_WRITE,
                        .src_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = old_image_info.mip_level_count,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_id = texture_upload_info.old_image,
                    });

                    cmd_recorder.pipeline_barrier_image_transition({
                        .src_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
                        .dst_access = daxa::AccessConsts::READ_WRITE,
                        .src_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                        .dst_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = image_info.mip_level_count,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                        .image_id = texture_upload_info.dst_image,
                    });
                }

                if(context->device.is_image_id_valid(texture_upload_info.old_image)) {
                    context->destroy_image_deferred(cmd_recorder, texture_upload_info.old_image);
                }

                if(context->device.is_buffer_id_valid(texture_upload_info.staging_buffer)) {
                    context->destroy_buffer_deferred(cmd_recorder, texture_upload_info.staging_buffer);
                }
            }
        }

        ret.upload_commands = cmd_recorder.complete_current_commands();
        return ret;
    }
}
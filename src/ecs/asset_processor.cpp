#include "asset_processor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <meshoptimizer.h>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <shader_library/vertex_compression.inl>
#include <random>
#include <utils/zstd.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>
#include <utils/file_io.hpp>

#include <numeric>
#include <metis.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <nvtt/nvtt.h>
#pragma clang diagnostic pop
#endif

static constexpr i32 TARGET_MESHLETS_PER_GROUP = 8;
static constexpr f32 SIMPLIFICATION_FAILURE_PERCENTAGE = 0.95f;

namespace foundation {
    AssetProcessor::AssetProcessor(Context* _context) : context{_context}, mesh_upload_mutex{std::make_unique<std::mutex>()} { PROFILE_SCOPE; }
    AssetProcessor::~AssetProcessor() = default;

    template <typename ElemT, bool IS_INDEX_BUFFER>
    auto load_data(fastgltf::Asset& asset, fastgltf::Accessor& accessor) {
        std::vector<ElemT> ret(accessor.count);
        if constexpr(IS_INDEX_BUFFER) {
            if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                std::vector<u16> u16_index_buffer(accessor.count);
                fastgltf::copyFromAccessor<u16>(asset, accessor, u16_index_buffer.data());
                for (usize i = 0; i < u16_index_buffer.size(); ++i) {
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

    void AssetProcessor::convert_gltf_to_binary(const std::filesystem::path& input_path, const std::filesystem::path& output_path) {
        if(!std::filesystem::exists(input_path)) {
            throw std::runtime_error("couldnt not find model: " + input_path.string());
        }

        std::unique_ptr<fastgltf::Asset> asset = {};
        {
            fastgltf::Parser parser{};
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;

            auto gltf_file = fastgltf::MappedGltfFile::FromPath(input_path);
            
            if(gltf_file.error() != fastgltf::Error::None) {
                throw std::runtime_error("something went wrong");
            }
            
            auto fastgltf_asset = parser.loadGltf(gltf_file.get(), input_path.parent_path(), gltf_options);
            asset = std::make_unique<fastgltf::Asset>(std::move(fastgltf_asset.get()));
        }

        std::vector<BinaryNode> binary_nodes = {};
        for(const auto& node : asset->nodes) {
            std::vector<u32> children = {};
            for(usize child : node.children) {
                children.push_back(s_cast<u32>(child));
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

        glm::vec3 model_aabb_max = glm::vec3{std::numeric_limits<f32>::lowest()};
        glm::vec3 model_aabb_min = glm::vec3{std::numeric_limits<f32>::max()};

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_offset = s_cast<u32>(binary_meshes.size());
            binary_meshes.reserve(binary_meshes.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                std::vector<std::byte> compressed_data = {};

                u32 mesh_meshlet_count = {};
                {
                    ProcessedMeshInfo processed_mesh_info = AssetProcessor::process_mesh({
                        .asset = asset.get(),
                        .gltf_mesh_index = mesh_index,
                        .gltf_primitive_index = primitive_index,
                    });

                    const auto& mesh_aabb = processed_mesh_info.mesh_aabb;
                    model_aabb_max = glm::max(model_aabb_max, mesh_aabb.center + mesh_aabb.extent);
                    model_aabb_min = glm::min(model_aabb_max, mesh_aabb.center - mesh_aabb.extent);

                    meshlet_count += s_cast<u32>(processed_mesh_info.meshlets.size());
                    vertex_count += s_cast<u32>(processed_mesh_info.positions.size());
                    mesh_meshlet_count = s_cast<u32>(processed_mesh_info.meshlets.size());

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

                std::optional<u32> material_index = std::nullopt;
                if(gltf_mesh.primitives[primitive_index].materialIndex.has_value()) {
                    material_index = std::make_optional(s_cast<u32>(gltf_mesh.primitives[primitive_index].materialIndex.value()));
                }

                binary_meshes.push_back(BinaryMesh {
                    .material_index = material_index,
                    .meshlet_count = mesh_meshlet_count,
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
                .albedo_factor = { material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1], material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3] },
                .metallic_factor = material.pbrData.metallicFactor,
                .roughness_factor = material.pbrData.roughnessFactor,
                .emissive_factor = { material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2] },
                .alpha_mode = s_cast<BinaryAlphaMode>(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided,
                .name = std::string{material.name.c_str()},
            });
        }

        for (u32 i = 0; i < s_cast<u32>(asset->images.size()); ++i) {
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
                create_alpha_mask |= material.alpha_mode != BinaryAlphaMode::Opaque;
            }

            const auto& image = asset->images[i];

            auto get_data = [&](this auto&& self, const fastgltf::DataSource& data) -> std::vector<std::byte> {
                return std::visit(fastgltf::visitor {
                    [&](const std::monostate&) -> std::vector<std::byte>  {
                        ASSERT(false, "std::monostate should never happen");
                        return {};
                    },
                    [&](const fastgltf::sources::BufferView& source) -> std::vector<std::byte>  {
                        fastgltf::BufferView& buffer_view = asset->bufferViews[source.bufferViewIndex];
                        fastgltf::Buffer& buffer = asset->buffers[buffer_view.bufferIndex];
                        std::vector<std::byte> buffer_data = self(buffer.data);

                        std::vector<std::byte> ret = {};
                        ret.resize(buffer_view.byteLength);
                        std::memcpy(ret.data(), buffer_data.data() + buffer_view.byteOffset, buffer_view.byteLength);
                        return ret;
                    },
                    [&](const fastgltf::sources::URI& source) -> std::vector<std::byte>  {
                        std::filesystem::path path{source.uri.path().begin(), source.uri.path().end()};
                        if(source.uri.isLocalPath()) { path = input_path.parent_path() / path; }
                        return read_file_to_bytes(path);
                    },
                    [&](const fastgltf::sources::Array& source) -> std::vector<std::byte>  {
                        std::vector<std::byte> ret = {};
                        ret.resize(source.bytes.size_bytes());
                        std::memcpy(ret.data(), source.bytes.data(), source.bytes.size_bytes());
                        return ret;
                    },
                    [&](const fastgltf::sources::Vector& source) -> std::vector<std::byte>  {
                        std::vector<std::byte> ret = {};
                        ret.resize(source.bytes.size());
                        std::memcpy(ret.data(), source.bytes.data(), source.bytes.size());
                        return ret;
                    },
                    [&](const fastgltf::sources::CustomBuffer& /* source */) -> std::vector<std::byte>  {
                        ASSERT(false, "fastgltf::sources::CustomBuffer isnt handled");
                        return {};
                    },
                    [&](const fastgltf::sources::ByteView& source) -> std::vector<std::byte>  {
                        std::vector<std::byte> ret = {};
                        ret.resize(source.bytes.size());
                        std::memcpy(ret.data(), source.bytes.data(), source.bytes.size());
                        return ret;
                    },
                    [&](const fastgltf::sources::Fallback& /* source */) -> std::vector<std::byte>  {
                        ASSERT(false, "fastgltf::sources::Fallback isnt handled");
                        return {};
                    },
                }, data);
            };

            std::vector<std::byte> raw_data = {};
            i32 width = 0;
            i32 height = 0;
            i32 num_channels = 0;

            {
                auto gltf_data = get_data(image.data);
                u8* image_data = stbi_load_from_memory(r_cast<const u8*>(gltf_data.data()), s_cast<i32>(gltf_data.size()), &width, &height, &num_channels, 4);
                if(image_data == nullptr) { std::println("bozo"); }
                raw_data.resize(s_cast<u64>(width * height * 4));
                std::memcpy(raw_data.data(), image_data, s_cast<u64>(width * height * 4));
                stbi_image_free(image_data);
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
                            if(binary_material.alpha_mode == BinaryAlphaMode::Masked) {
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
                        
                        if(material.alpha_mode != BinaryAlphaMode::Opaque) {
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

            std::println("[{} / {}] - image - done", i+1, asset->images.size());
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
                .model_aabb = {
                    .center = (model_aabb_min + model_aabb_max) * 0.5f,
                    .extent = (model_aabb_max - model_aabb_min) * 0.5f,
                }
            });

            byte_writer.write(BinaryAssetInfo {
                .nodes = binary_nodes,
                .mesh_groups = binary_mesh_groups,
                .meshes = binary_meshes,
                .materials = binary_materials,
                .textures = binary_textures,
            });

            std::println("writer {}", byte_writer.data.size());
            compressed_data = zstd_compress(byte_writer.data, 14);
        }
    
        write_bytes_to_file(compressed_data, output_path);
    }

    struct PairHasher {
        template <class T1, class T2>
        auto operator()(const std::pair<T1,T2>& pair) const -> usize {
            usize hash = std::hash<usize>()(s_cast<usize>(pair.first));
            hash ^= std::hash<usize>()(pair.second) + 0x9e3779b9 + (hash<<6) + (hash>>2);
            return hash;
        }
    };
#pragma region FIND CONNECTED MESHLETS
    using MeshletPair = std::pair<u32, u32>;
    using MeshletCountPair = std::pair<u32, u32>;

    struct FindConnectedMeshletsInfo {
        const std::vector<u32>& simplification_queue;
        const std::vector<Meshlet>& meshlets;
        const std::vector<u32>& meshlet_indirect_vertices;
        const std::vector<u8>& meshlet_micro_indices;
        const std::vector<glm::vec3>& positions;
    };

    static auto find_connected_meshlets(const FindConnectedMeshletsInfo& info) -> std::vector<std::vector<MeshletCountPair>> {
        std::vector<std::vector<u32>> vertices_to_meshlets = {};
        vertices_to_meshlets.resize(info.positions.size());

        u32 meshlet_queue_index = 0;
        for(const u32 meshlet_index : info.simplification_queue) {
            const Meshlet& meshlet = info.meshlets[meshlet_index];

            for (u32 i = 0; i < meshlet.vertex_count; ++i) {
                u32 vertex_index = info.meshlet_indirect_vertices[meshlet.indirect_vertex_offset + i];
                std::vector<u32>& vertex_to_meshlet = vertices_to_meshlets[vertex_index];
                if(vertex_to_meshlet.empty() || vertex_to_meshlet.back() != meshlet_queue_index) {
                    vertex_to_meshlet.push_back(meshlet_queue_index);
                }
            }

            meshlet_queue_index++;
        }

        auto create_combinations = [](const std::vector<u32>& vec) -> std::vector<std::pair<u32, u32>> {
            std::vector<std::pair<u32, u32>> combinations = {};
            for(u32 i = 0; i < vec.size(); i++) {
                for(u32 j = i + 1; j < vec.size(); j++) {
                    combinations.emplace_back(vec[i], vec[j]);
                }
            }
            return combinations;
        };

        std::unordered_map<MeshletPair, u32, PairHasher> meshlet_pair_to_shared_vertex_count = {};
        for(auto& vertex_meshlets_ids : vertices_to_meshlets) {
            std::vector<std::pair<u32, u32>> meshlet_pairs = create_combinations(vertex_meshlets_ids);
            for(auto [meshlet_queue_id_1, meshlet_queue_id_2] : meshlet_pairs) {
                MeshletPair key = MeshletPair{glm::min(meshlet_queue_id_1, meshlet_queue_id_2), glm::max(meshlet_queue_id_1, meshlet_queue_id_2)};

                auto entry = meshlet_pair_to_shared_vertex_count.find(key);
                if(entry != meshlet_pair_to_shared_vertex_count.end()) {
                    entry->second += 1;
                } else {
                    meshlet_pair_to_shared_vertex_count.insert({key, 1});
                }
            }
        }

        std::vector<std::vector<MeshletCountPair>> connected_meshlets_per_meshlet = {};
        connected_meshlets_per_meshlet.resize(info.simplification_queue.size());

        for(auto [meshlet_queue_ids, shared_count] : meshlet_pair_to_shared_vertex_count) {
            auto [meshlet_queue_id_1, meshlet_queue_id_2] = meshlet_queue_ids;

            connected_meshlets_per_meshlet[meshlet_queue_id_1].emplace_back(meshlet_queue_id_2, shared_count);
            connected_meshlets_per_meshlet[meshlet_queue_id_2].emplace_back(meshlet_queue_id_1, shared_count);
        }

        for(auto& vec : connected_meshlets_per_meshlet) {
            std::sort(vec.begin(), vec.end(), [](MeshletCountPair pair_1, MeshletCountPair pair_2) { return pair_1.first < pair_2.first; });
        }

        return connected_meshlets_per_meshlet;
    }
#pragma endregion

#pragma region GROUP MESHLETS
    struct GroupMeshletsInfo {
        const std::vector<std::vector<MeshletCountPair>>& connected_meshlets_per_meshlet = {};
        const std::vector<u32>& simplification_queue = {};
    };

    static auto group_meshlets(const GroupMeshletsInfo& info) -> std::vector<std::vector<u32>> {
        std::vector<i32> xadj = {};
        xadj.reserve(info.simplification_queue.size() + 1);

        std::vector<i32> adjncy = {};
        std::vector<i32> adjwgt = {};

        for(const auto& connceted_meshlets : info.connected_meshlets_per_meshlet) {
            xadj.push_back(s_cast<i32>(adjncy.size()));
            for(auto [meshlet_id, shared_count] : connceted_meshlets) {
                adjncy.push_back(s_cast<i32>(meshlet_id));
                adjwgt.push_back(s_cast<i32>(shared_count));
            }
        }

        xadj.push_back(s_cast<i32>(adjncy.size()));

        std::array<idx_t, METIS_NOPTIONS> options = {};
        METIS_SetDefaultOptions(options.data());
        options[METIS_OPTION_SEED] = 17;

        i32 ncon = 1;
        i32 partition_count = s_cast<i32>(round_up_div(s_cast<u32>(info.simplification_queue.size()), s_cast<u32>(TARGET_MESHLETS_PER_GROUP)));
        i32 nvtxs = s_cast<i32>(xadj.size()) - 1;
        i32 edgecut = 0;

        // special case for METIS
        if(partition_count == 1) {
            return { info.simplification_queue };
        }

        std::vector<i32> group_per_meshlet = {};
        group_per_meshlet.resize(info.simplification_queue.size());

        METIS_PartGraphKway(&nvtxs, 
            &ncon, 
            xadj.data(), 
            adjncy.data(), 
            nullptr, 
            nullptr, 
            adjwgt.data(), 
            &partition_count, 
            nullptr, 
            nullptr, 
            options.data(), 
            &edgecut, 
            group_per_meshlet.data());


        std::vector<std::vector<u32>> groups = {};
        groups.resize(s_cast<usize>(partition_count));

        u32 meshlet_queue_id = 0;
        for(const i32 meshlet_group : group_per_meshlet) {
            groups[s_cast<usize>(meshlet_group)].push_back(info.simplification_queue[meshlet_queue_id]);
            meshlet_queue_id++;
        }
        
        return groups;
    };
#pragma endregion

#pragma region LOCK BORDERS
    struct LockGroupBordersInfo {
        std::vector<u8>& vertex_locks;
        const std::vector<std::vector<u32>>& groups;
        const std::vector<Meshlet>& meshlets;
        const std::vector<u32>& meshlet_indirect_vertices;
        const std::vector<glm::vec3>& positions;
    };

    static void lock_group_borders(const LockGroupBordersInfo& info) {
        std::vector<i32> locks = {};
        locks.resize(info.positions.size());
        std::fill(locks.begin(), locks.end(), -1);

        u32 group_id = 0;
        for(const std::vector<u32>& meshlets : info.groups) {
            for(u32 meshlet_id = 0; meshlet_id < meshlets.size(); meshlet_id++) {
                const Meshlet& meshlet = info.meshlets[meshlet_id];

                for (u32 i = 0; i < meshlet.vertex_count; ++i) {
                    u32 vertex_id = info.meshlet_indirect_vertices[meshlet.indirect_vertex_offset + i];
                    if(locks[vertex_id] == -1 || locks[vertex_id] == s_cast<i32>(group_id)) {
                        locks[vertex_id] = s_cast<i32>(group_id);
                    } else {
                        locks[vertex_id] = -2;
                    }
                }
            }

            group_id++;
        }

        for(u32 i = 0; i < info.positions.size(); i++) {
            info.vertex_locks[i] = s_cast<u8>(locks[i] == -2);
        }
    };
#pragma endregion

#pragma region SIMPLIFY MESHLET GROUP
    struct SimplifyMeshletGroupInfo {
        const std::vector<u32>& group_meshlets;
        const std::vector<Meshlet>& meshlets;
        const std::vector<u32>& meshlet_indirect_vertices;
        const std::vector<u8>& meshlet_micro_indices;
        const std::vector<glm::vec3>& positions;
        const std::vector<glm::vec3>& normals;
        const std::vector<u8>& vertex_locks;
    };

    static auto simplify_meshlet_group(const SimplifyMeshletGroupInfo& info) -> std::optional<std::pair<std::vector<u32>, f32>> {
        std::vector<u32> group_indices = {};

        for(const u32 meshlet_id : info.group_meshlets) {
            const Meshlet& meshlet = info.meshlets[meshlet_id];

            for(u32 triangle_index = 0; triangle_index < meshlet.triangle_count; triangle_index++) {
                for(u32 triangle_corner_index = 0; triangle_corner_index < 3; triangle_corner_index++) {
                    u32 micro_index = info.meshlet_micro_indices[meshlet.micro_indices_offset + triangle_index * 3 + triangle_corner_index];
                    group_indices.push_back(info.meshlet_indirect_vertices[meshlet.indirect_vertex_offset + micro_index]);

                }
            }
        }

        std::array<f32, 3> attribute_weights = { 0.5f, 0.5f, 0.5f };

        f32 error = 0.0f;
        std::vector<u32> simplified_group_indices = {};
        simplified_group_indices.resize(group_indices.size());
        usize index_count = meshopt_simplifyWithAttributes(
            simplified_group_indices.data(), 
            group_indices.data(), 
            group_indices.size(), 
            r_cast<const f32*>(info.positions.data()), 
            info.positions.size(), 
            sizeof(glm::vec3),
            r_cast<const f32*>(info.normals.data()),
            sizeof(glm::vec3),
            attribute_weights.data(),
            attribute_weights.size(),
            info.vertex_locks.data(), 
            group_indices.size() / 2, 
            std::numeric_limits<f32>::max(), 
            meshopt_SimplifySparse | meshopt_SimplifyErrorAbsolute | meshopt_SimplifyLockBorder, 
            &error
        );
        simplified_group_indices.resize(index_count);

        if(s_cast<f32>(simplified_group_indices.size()) / s_cast<f32>(group_indices.size()) > SIMPLIFICATION_FAILURE_PERCENTAGE) {
            std::println("new {} original {}", simplified_group_indices.size(), group_indices.size());
            std::println("retry % {} error % {}", (s_cast<f32>(simplified_group_indices.size()) / s_cast<f32>(group_indices.size())), error);
            return std::nullopt;
        }

        return {{simplified_group_indices, error}};
    }
#pragma endregion
    
#pragma region COMPUTE LOD GROUP DATA 
    struct ComputeLODGroupDataInfo {
        const std::vector<u32>& group_meshlets;
        f32& group_error;
        std::vector<MeshletBoundingSpheres>& bounding_spheres;
        std::vector<MeshletSimplificationError>& simplification_errors;
    };

    static auto compute_lod_group_data(ComputeLODGroupDataInfo& info) -> BoundingSphere {
#if 0        
        BoundingSphere group_bounding_sphere = BoundingSphere {
            .center = { 0.0f, 0.0f, 0.0f },
            .radius = 0.0f
        };

        f32 weight = 0.0f;
        for(const u32 meshlet_id : info.group_meshlets) {
            BoundingSphere meshlet_lod_bounding_sphere = info.bounding_spheres[meshlet_id].lod_group_sphere;
            group_bounding_sphere.center += meshlet_lod_bounding_sphere.center * meshlet_lod_bounding_sphere.radius;
            weight += meshlet_lod_bounding_sphere.radius;
        }
        group_bounding_sphere.center /= weight;

        
        for(const u32 meshlet_id : info.group_meshlets) {
            BoundingSphere meshlet_lod_bounding_sphere = info.bounding_spheres[meshlet_id].lod_group_sphere;
            f32 d = glm::distance(meshlet_lod_bounding_sphere.center, group_bounding_sphere.center);
            group_bounding_sphere.radius = glm::max(meshlet_lod_bounding_sphere.radius + d, group_bounding_sphere.radius);
        }
#else
        std::vector<BoundingSphere> group_bounding_spheres = {};
        group_bounding_spheres.reserve(info.group_meshlets.size());
        for(const u32 meshlet_id : info.group_meshlets) {
            group_bounding_spheres.push_back(info.bounding_spheres[meshlet_id].lod_group_sphere);
        }

        BoundingSphere group_bounding_sphere = y_cast<BoundingSphere>(meshopt_computeSphereBounds(
            &group_bounding_spheres[0].center[0], 
            group_bounding_spheres.size(), 
            sizeof(BoundingSphere), 
            &group_bounding_spheres[0].radius, 
            sizeof(BoundingSphere)
        ));
#endif

        for(const u32 meshlet_id : info.group_meshlets) {
            info.group_error = glm::max(info.simplification_errors[meshlet_id].group_error, info.group_error);
        }

        for(const u32 meshlet_id : info.group_meshlets) {
            info.bounding_spheres[meshlet_id].lod_parent_group_sphere = group_bounding_sphere;
            info.simplification_errors[meshlet_id].parent_group_error = info.group_error;
        }

        return group_bounding_sphere;
    }
#pragma endregion

#pragma region SPLIT SIMPLIFIED GROUP INTO MESHLETS
    struct SplitSimplifiedGroupIntoNewMeshletsInfo {
        const std::vector<u32>& simplified_group_indices;
        const std::vector<glm::vec3>& positions;
        std::vector<u32>& meshlet_indirect_vertices;
        std::vector<u8>& meshlet_micro_indices;
        std::vector<AABB>& aabbs;
        std::vector<Meshlet>& meshlets;
    };

    static auto split_simplified_group_into_new_meshlets(SplitSimplifiedGroupIntoNewMeshletsInfo& info) -> std::pair<u32, std::vector<BoundingSphere>> {
        auto ret = AssetProcessor::generate_meshlets(GenerateMeshletsInfo {
            .indices = info.simplified_group_indices,
            .positions = info.positions
        });

        u32 indirect_vertices_offset = s_cast<u32>(info.meshlet_indirect_vertices.size());
        u32 micro_indices_offset = s_cast<u32>(info.meshlet_micro_indices.size());

        info.meshlet_indirect_vertices.insert(info.meshlet_indirect_vertices.end(), ret.indirect_vertices.begin(), ret.indirect_vertices.end());
        info.meshlet_micro_indices.insert(info.meshlet_micro_indices.end(), ret.micro_indices.begin(), ret.micro_indices.end());
    
        for(Meshlet& meshlet : ret.meshlets) {
            meshlet.indirect_vertex_offset += indirect_vertices_offset;
            meshlet.micro_indices_offset += micro_indices_offset;
        }

        info.meshlets.insert(info.meshlets.end(), ret.meshlets.begin(), ret.meshlets.end());
        info.aabbs.insert(info.aabbs.end(), ret.aabbs.begin(), ret.aabbs.end());

        return { s_cast<u32>(ret.meshlets.size()), ret.bounding_spheres };
    }
#pragma endregion

#pragma region GENERATE MESHLETS    
    auto AssetProcessor::generate_meshlets(const GenerateMeshletsInfo& info) -> ProcessedMeshletsInfo {
        ProcessedMeshletsInfo ret = {};
        
        constexpr usize MAX_VERTICES = MAX_VERTICES_PER_MESHLET;
        constexpr usize MAX_TRIANGLES = MAX_TRIANGLES_PER_MESHLET;
        constexpr f32 CONE_WEIGHT = 1.0f;

        usize max_meshlets = meshopt_buildMeshletsBound(info.indices.size(), MAX_VERTICES, MAX_TRIANGLES);
        ret.meshlets.resize(max_meshlets);
        ret.indirect_vertices.resize(max_meshlets * MAX_VERTICES);
        ret.micro_indices.resize(max_meshlets * MAX_TRIANGLES * 3);

        usize meshlet_count = meshopt_buildMeshlets(
            r_cast<meshopt_Meshlet*>(ret.meshlets.data()),
            ret.indirect_vertices.data(),
            ret.micro_indices.data(),
            info.indices.data(),
            info.indices.size(),
            r_cast<const f32*>(info.positions.data()),
            info.positions.size(),
            sizeof(f32vec3),
            MAX_VERTICES,
            MAX_TRIANGLES,
            CONE_WEIGHT
        );

        const Meshlet& last = ret.meshlets[meshlet_count - 1];
        ret.indirect_vertices.resize(last.indirect_vertex_offset + last.vertex_count);
        ret.micro_indices.resize(last.micro_indices_offset + ((last.triangle_count * 3u + 3u) & ~3u));
        ret.meshlets.resize(meshlet_count);

        for(const Meshlet& meshlet : ret.meshlets) {
            meshopt_optimizeMeshlet(
                &ret.indirect_vertices[meshlet.indirect_vertex_offset], 
                &ret.micro_indices[meshlet.micro_indices_offset], 
                meshlet.triangle_count, 
                meshlet.vertex_count
            );
        }

        ret.bounding_spheres.resize(meshlet_count);
        ret.aabbs.resize(meshlet_count);

        f32vec3 mesh_aabb_max = glm::vec3{std::numeric_limits<f32>::lowest()};
        f32vec3 mesh_aabb_min = glm::vec3{std::numeric_limits<f32>::max()};

        for(usize meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
            const auto& meshlet = ret.meshlets[meshlet_index];

            meshopt_Bounds raw_bounds = meshopt_computeMeshletBounds(
                &ret.indirect_vertices[meshlet.indirect_vertex_offset],
                &ret.micro_indices[meshlet.micro_indices_offset],
                meshlet.triangle_count,
                r_cast<const f32*>(info.positions.data()),
                info.positions.size(),
                sizeof(glm::vec3)
            );

            ret.bounding_spheres[meshlet_index] = BoundingSphere {
                .center = { raw_bounds.center[0], raw_bounds.center[1], raw_bounds.center[2]},
                .radius = raw_bounds.radius
            };

            glm::vec3 min_pos = info.positions[ret.indirect_vertices[meshlet.indirect_vertex_offset]];
            glm::vec3 max_pos = info.positions[ret.indirect_vertices[meshlet.indirect_vertex_offset]];

            for (u32 i = 0; i < meshlet.vertex_count; ++i) {
                glm::vec3 pos = info.positions[ret.indirect_vertices[meshlet.indirect_vertex_offset + i]];
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }

            mesh_aabb_min = glm::min(mesh_aabb_min, min_pos);
            mesh_aabb_max = glm::max(mesh_aabb_max, max_pos);

            ret.aabbs[meshlet_index].center = (max_pos + min_pos) * 0.5f;
            ret.aabbs[meshlet_index].extent =  (max_pos - min_pos) * 0.5f;
        }

        ret.mesh_aabb = {
            .center = (mesh_aabb_max + mesh_aabb_min) * 0.5f,
            .extent =  (mesh_aabb_max - mesh_aabb_min) * 0.5f,
        };

        return ret;
    }
#pragma endregion

#pragma region GENERATE INDEX BUFFER
    auto AssetProcessor::generate_index_buffer(const GenerateIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        std::vector<meshopt_Stream> streams = {};
        streams.push_back(meshopt_Stream { .data = info.unindexed_positions.data(), .size = info.unindexed_positions.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_normals.data(), .size = info.unindexed_normals.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_uvs.data(), .size = info.unindexed_uvs.size(), .stride = sizeof(f32vec2) });
        
        usize index_count = info.unindexed_positions.size();
        usize unindexed_vertex_count = info.unindexed_positions.size();
        
        std::vector<u32> remap_table(unindexed_vertex_count);
        usize unique_vertex_count = meshopt_generateVertexRemapMulti(remap_table.data(), nullptr, index_count, unindexed_vertex_count, streams.data(), streams.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.index_buffer.resize(index_count);

        meshopt_remapVertexBuffer(ret.positions.data(), ret.positions.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), ret.normals.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), ret.uvs.data(), unindexed_vertex_count, sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.index_buffer.data(), nullptr, index_count, remap_table.data());
    
        return ret;
    }
#pragma endregion

#pragma region OPTIMIZE VERTEX CACHE
    auto AssetProcessor::optimize_vertex_cache(const std::vector<u32>& index_buffer, u32 vertex_count) -> std::vector<u32> {
        std::vector<u32> optimized_index_buffer(index_buffer.size());
        meshopt_optimizeVertexCache(optimized_index_buffer.data(), index_buffer.data(), index_buffer.size(), vertex_count);
        return optimized_index_buffer;
    }
#pragma endregion

#pragma region OPTIMIZE VERTEX FETCH
    auto AssetProcessor::optimize_vertex_fetch(const ProcessedIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        std::vector<u32> remap_table(info.positions.size());
        usize unique_vertex_count = meshopt_optimizeVertexFetchRemap(remap_table.data(), info.index_buffer.data(), info.index_buffer.size(), info.positions.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.index_buffer.resize(info.index_buffer.size());

        meshopt_remapVertexBuffer(ret.positions.data(), info.positions.data(), info.positions.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), info.normals.data(), info.normals.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), info.uvs.data(), info.uvs.size(), sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.index_buffer.data(), info.index_buffer.data(), info.index_buffer.size(), remap_table.data());
    
        return ret;
    }
#pragma endregion

#pragma region PROCESS MESH
    auto AssetProcessor::process_mesh(const ProcessMeshInfo &info) -> ProcessedMeshInfo {
        fastgltf::Asset& gltf_asset = *info.asset;
        fastgltf::Mesh& gltf_mesh = info.asset->meshes[info.gltf_mesh_index];
        fastgltf::Primitive& gltf_primitive = gltf_mesh.primitives[info.gltf_primitive_index];

        std::vector<glm::vec3> vert_positions = {};
        auto* position_attribute_iter = gltf_primitive.findAttribute("POSITION");
        if(position_attribute_iter != gltf_primitive.attributes.end()) {
            vert_positions = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[position_attribute_iter->accessorIndex]);
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
        std::vector<glm::vec3> vert_normals = {};
        {
            auto* normal_attribute_iter = gltf_primitive.findAttribute("NORMAL");
            if(normal_attribute_iter != gltf_primitive.attributes.end()) {
                vert_normals = load_data<glm::vec3, false>(gltf_asset, gltf_asset.accessors[normal_attribute_iter->accessorIndex]);
            } else {
                fill(vert_normals, vertex_count);
            }

            packed_normals.reserve(packed_normals.size());
            for(const f32vec3& normal : vert_normals) {
                packed_normals.push_back(encode_normal(normal));
            }
        }

        std::vector<u32> packed_uvs = {};
        std::vector<glm::vec2> vert_uvs = {};
        {
            auto* uvs_attribute_iter = gltf_primitive.findAttribute("TEXCOORD_0");
            if(uvs_attribute_iter != gltf_primitive.attributes.end()) {
                vert_uvs = load_data<glm::vec2, false>(gltf_asset, gltf_asset.accessors[uvs_attribute_iter->accessorIndex]);
            } else {
                fill(vert_uvs, vertex_count);
            }

            packed_uvs.reserve(vert_uvs.size());
            for(const f32vec2& uv : vert_uvs) {
                packed_uvs.push_back(encode_uv(uv));
            }
        }

        std::vector<u32> indices = {};
        if(gltf_primitive.indicesAccessor.has_value()) {
            indices = load_data<u32, true>(gltf_asset, gltf_asset.accessors[gltf_primitive.indicesAccessor.value()]);
        } else {
            ProcessedIndexBufferInfo ret = generate_index_buffer(GenerateIndexBufferInfo {
                .unindexed_positions = vert_positions,
                .unindexed_normals = vert_normals,
                .unindexed_uvs = vert_uvs
            });

            vert_positions = ret.positions;
            vert_normals = ret.normals;
            vert_uvs = ret.uvs;
            indices = ret.index_buffer;
        }
        
        {
            ProcessedIndexBufferInfo ret = optimize_vertex_fetch(ProcessedIndexBufferInfo {
                .positions = vert_positions,
                .normals = vert_normals,
                .uvs = vert_uvs,
                .index_buffer = indices
            });

            vert_positions = ret.positions;
            vert_normals = ret.normals;
            vert_uvs = ret.uvs;
            indices = ret.index_buffer;
        }

        indices = optimize_vertex_cache(indices, vertex_count);

        std::vector<Meshlet> meshlets = {};
        std::vector<u32> meshlet_indirect_vertices = {};
        std::vector<u8> meshlet_micro_indices = {};
        std::vector<MeshletBoundingSpheres> bounding_spheres = {};
        std::vector<AABB> meshlet_aabbs = {};
        AABB mesh_aabb = {};

        {
            ProcessedMeshletsInfo ret = generate_meshlets(GenerateMeshletsInfo {
                .indices = indices,
                .positions = vert_positions
            });

            meshlets = ret.meshlets;
            meshlet_indirect_vertices = ret.indirect_vertices;
            meshlet_micro_indices = ret.micro_indices;
            
            for(const auto& bounding_sphere : ret.bounding_spheres) {
                bounding_spheres.push_back(MeshletBoundingSpheres {
                    .culling_sphere = bounding_sphere,
                    .lod_group_sphere = bounding_sphere,
                    .lod_parent_group_sphere = {
                        .center = glm::vec3{0.0f},
                        .radius = 0.0f,
                    },
                });
            }

            meshlet_aabbs = ret.aabbs;
            mesh_aabb = ret.mesh_aabb;
        }
        
        std::vector<MeshletSimplificationError> simplification_errors = {};
        simplification_errors.resize(meshlets.size());
        std::fill(simplification_errors.begin(), simplification_errors.end(), MeshletSimplificationError {
            .group_error = 0.0f,
            .parent_group_error = std::numeric_limits<f32>::max()
        });

        std::vector<u8> vertex_locks = {};
        vertex_locks.resize(vert_positions.size());
        std::fill(vertex_locks.begin(), vertex_locks.end(), 0);

        std::vector<u32> simplification_queue = {};
        simplification_queue.resize(meshlets.size());
        std::iota(simplification_queue.begin(), simplification_queue.end(), 0);

        std::vector<u32> retry_queue = {};

        while(simplification_queue.size() > 1) {
            std::vector<std::vector<MeshletCountPair>> connected_meshlets_per_meshlet = find_connected_meshlets(FindConnectedMeshletsInfo {
                .simplification_queue = simplification_queue,
                .meshlets = meshlets,
                .meshlet_indirect_vertices = meshlet_indirect_vertices,
                .meshlet_micro_indices = meshlet_micro_indices,
                .positions = vert_positions,
            });

            std::vector<std::vector<u32>> groups = group_meshlets(GroupMeshletsInfo {
                .connected_meshlets_per_meshlet = connected_meshlets_per_meshlet,
                .simplification_queue = simplification_queue,
            });

            lock_group_borders(LockGroupBordersInfo {
                .vertex_locks = vertex_locks,
                .groups = groups,
                .meshlets = meshlets,
                .meshlet_indirect_vertices = meshlet_indirect_vertices,
                .positions = vert_positions,
            });

            u32 next_lod_start = s_cast<u32>(meshlets.size());

            for(const std::vector<u32>& group_meshlets : groups) {
                if(group_meshlets.size() == 1) { 
                    retry_queue.push_back(group_meshlets[0]); 
                    continue; 
                }

                std::optional<std::pair<std::vector<u32>, f32>> simplification_result = simplify_meshlet_group(SimplifyMeshletGroupInfo {
                    .group_meshlets = group_meshlets,
                    .meshlets = meshlets,
                    .meshlet_indirect_vertices = meshlet_indirect_vertices,
                    .meshlet_micro_indices = meshlet_micro_indices,
                    .positions = vert_positions,
                    .normals = vert_normals,
                    .vertex_locks = vertex_locks,
                });

                if(!simplification_result.has_value()) {
                    retry_queue.insert(retry_queue.end(), group_meshlets.begin(), group_meshlets.end());
                    std::println("retry");
                    continue;
                }

                auto [simplified_group_indices, group_error] = simplification_result.value();
            
                auto compute_lod_group_data_info = ComputeLODGroupDataInfo {
                    .group_meshlets = group_meshlets,
                    .group_error = group_error,
                    .bounding_spheres = bounding_spheres,
                    .simplification_errors = simplification_errors
                };

                BoundingSphere group_bounding_sphere = compute_lod_group_data(compute_lod_group_data_info);

                auto split_simplified_group_into_new_meshlets_info = SplitSimplifiedGroupIntoNewMeshletsInfo {
                    .simplified_group_indices = simplified_group_indices,
                    .positions = vert_positions,
                    .meshlet_indirect_vertices = meshlet_indirect_vertices,
                    .meshlet_micro_indices = meshlet_micro_indices,
                    .aabbs = meshlet_aabbs,
                    .meshlets = meshlets
                };

                auto [group_meshlet_count, group_meshlets_bounding_spheres] = split_simplified_group_into_new_meshlets(split_simplified_group_into_new_meshlets_info);

                u32 new_meshlets_start = s_cast<u32>(meshlets.size()) - group_meshlet_count;
                bounding_spheres.resize(bounding_spheres.size() + group_meshlet_count);
                for(u32 group_meshlet_id = 0; group_meshlet_id < group_meshlet_count; group_meshlet_id++) {
                    bounding_spheres[new_meshlets_start + group_meshlet_id] = MeshletBoundingSpheres {
                        .culling_sphere = group_meshlets_bounding_spheres[group_meshlet_id],
                        .lod_group_sphere = group_bounding_sphere,
                        .lod_parent_group_sphere = {
                            .center = glm::vec3{0.0f},
                            .radius = 0.0f,
                        },
                    };
                }

                simplification_errors.resize(simplification_errors.size() + group_meshlet_count);
                for(u32 group_meshlet_id = 0; group_meshlet_id < group_meshlet_count; group_meshlet_id++) {
                    simplification_errors[new_meshlets_start + group_meshlet_id] = MeshletSimplificationError {
                        .group_error = group_error,
                        .parent_group_error = std::numeric_limits<f32>::max()
                    };
                }
            }

            simplification_queue.clear();
            simplification_queue.resize(s_cast<u32>(meshlets.size()) - next_lod_start);
            std::iota(simplification_queue.begin(), simplification_queue.end(), next_lod_start);
            if(!simplification_queue.empty()) {
                simplification_queue.insert(simplification_queue.end(), retry_queue.begin(), retry_queue.end());
                retry_queue.clear();
            }
        }

        return {
            .mesh_aabb = mesh_aabb,
            .positions = vert_positions,
            .normals = packed_normals,
            .uvs = packed_uvs,
            .meshlets = meshlets,
            .bounding_spheres = bounding_spheres,
            .simplification_errors = simplification_errors,
            .aabbs = meshlet_aabbs,
            .micro_indices = meshlet_micro_indices,
            .indirect_vertices = meshlet_indirect_vertices,
            .primitive_indices = indices
        };
    }
#pragma endregion

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
                    sizeof(MeshletBoundingSpheres) * processed_info.bounding_spheres.size() +
                    sizeof(MeshletSimplificationError) * processed_info.simplification_errors.size() +
                    sizeof(AABB) * processed_info.aabbs.size() +
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
                memcpy_data(mesh.bounding_spheres, processed_info.bounding_spheres);
                memcpy_data(mesh.simplification_errors, processed_info.simplification_errors);
                memcpy_data(mesh.meshlet_aabbs, processed_info.aabbs);
                memcpy_data(mesh.micro_indices, processed_info.micro_indices);
                memcpy_data(mesh.indirect_vertices, processed_info.indirect_vertices);
                memcpy_data(mesh.primitive_indices, processed_info.primitive_indices);
                memcpy_data(mesh.vertex_positions, processed_info.positions);
                memcpy_data(mesh.vertex_normals, processed_info.normals);
                memcpy_data(mesh.vertex_uvs, processed_info.uvs);
                
                mesh.mesh_buffer = mesh_buffer;

                const BinaryMesh& binary_mesh = info.asset->meshes[info.asset->mesh_groups[info.mesh_group_index].mesh_offset + info.mesh_index];
                if(binary_mesh.material_index.has_value()) {
                    mesh.material_index = info.material_manifest_offset + binary_mesh.material_index.value();
                } else {
                    mesh.material_index = INVALID_ID;
                }

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
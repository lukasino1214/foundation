#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(RenderMeshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct RenderMeshesPush {
    DAXA_TH_BLOB(RenderMeshes, uses)
    daxa_BufferPtr(TransformInfo) transform;
    daxa_BufferPtr(Vertex) vertices;
    daxa_BufferPtr(Material) material;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct RenderMeshesTask : RenderMeshes::Task {
    RenderMeshes::Task::AttachmentViews views = {};
    Shaper::Context* context = {};
    Shaper::Scene* scene = {};
    Shaper::AssetManager* asset_manager = {};
    RenderMeshesPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .vertex_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/traditional/tasks/render_meshes.slang" }, },
                .compile_options = {
                    .entry_point = "vertex_main",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{RenderMeshesTask::name()} + "_SHADER", "1" } },
                }
            },
            .fragment_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/traditional/tasks/render_meshes.slang" }, },
                .compile_options = { 
                    .entry_point = "fragment_main",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{RenderMeshesTask::name()} + "_SHADER", "1" } } 
                }
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::LESS_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT
            },
            .push_constant_size = static_cast<u32>(sizeof(RenderMeshesPush) + RenderMeshes::Task::attachment_shader_blob_size()),
            .name = std::string{RenderMeshesTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.info_image(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.info_image(ti.get(AT.u_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_image).ids[0].default_view(),
                    .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .depth_attachment = { daxa::RenderAttachmentInfo {
                .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue { .depth = 1.0f },
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(RenderMeshes::Task::name()));

        u32 counter = 0;
        scene->world->query<GlobalTransformComponent, MeshComponent>().each([&](GlobalTransformComponent& tc, MeshComponent& mc) {
            if(tc.buffer.is_empty()) { return; }
            if(mc.mesh_group_index.has_value()) {
                const auto& mesh_group_manifest = asset_manager->mesh_group_manifest_entries[mc.mesh_group_index.value()];
                for(u32 i = 0; i < mesh_group_manifest.mesh_count; i++) {
                    const auto& mesh_manifest = asset_manager->mesh_manifest_entries[mesh_group_manifest.mesh_manifest_indices_offset + i];
                    counter++;
                    if(mesh_manifest.render_info.has_value()) {
                        const auto& render_info = mesh_manifest.render_info.value();
                        push = RenderMeshesPush {
                            .transform = context->device.get_device_address(tc.buffer).value(),
                            .vertices = context->device.get_device_address(render_info.vertex_buffer).value(),
                            .material = context->device.get_device_address(render_info.material_buffer).value(),
                        };

                        assign_blob(push.uses, ti.attachment_shader_blob);
                        render_cmd.push_constant(push);

                        render_cmd.set_index_buffer(daxa::SetIndexBufferInfo {
                            .id = render_info.index_buffer,
                            .offset = 0,
                            .index_type = daxa::IndexType::uint32
                        });

                        render_cmd.draw_indexed(daxa::DrawIndexedInfo {
                            .index_count = render_info.index_count,
                            .instance_count = 1,
                            .first_index = 0,
                            .vertex_offset = 0,
                            .first_instance = 0,
                        });
                    }
                }
            }
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
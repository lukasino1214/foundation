#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsTransparentWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsTransparentWriteCommandPush {
    DAXA_TH_BLOB(DrawMeshletsTransparentWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMeshletsTransparentWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            "DrawMeshletsTransparentWriteCommand",
                                            DrawMeshletsTransparentWriteCommand::Task, 
                                            DrawMeshletsTransparentWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/draw_meshlets_transparent.slang", 
                                            "draw_meshlets_transparent_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsTransparent)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SunLight), u_sun)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TileData), u_tile_data)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_wboit_accumulation_image)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_wboit_reveal_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_TH_IMAGE_TYPED(READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsTransparentPush {
    DAXA_TH_BLOB(DrawMeshletsTransparent, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct DrawMeshletsTransparentTask : DrawMeshletsTransparent::Task {
    DrawMeshletsTransparent::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DrawMeshletsTransparentPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DrawMeshletsTransparent";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .mesh_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_transparent.slang" }, },
                .entry_point = "draw_meshlets_transparent_mesh",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTransparentTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_transparent.slang" }, },
                .entry_point = "draw_meshlets_transparent_frag",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTransparentTask::name()} + "_SHADER", "1" } } 
            },
            .task_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_transparent.slang" }, },
                .entry_point = "draw_meshlets_transparent_task",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTransparentTask::name()} + "_SHADER", "1" } },
            },
            .color_attachments = {
                daxa::RenderAttachment {
                    .format = daxa::Format::R16G16B16A16_SFLOAT,
                    .blend = { daxa::BlendInfo {
                        .src_color_blend_factor = daxa::BlendFactor::ONE,
                        .dst_color_blend_factor = daxa::BlendFactor::ONE,
                        .color_blend_op = daxa::BlendOp::ADD,
                        .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .dst_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .alpha_blend_op = daxa::BlendOp::ADD,
                    }},
                },
                daxa::RenderAttachment {
                    .format = daxa::Format::R16_SFLOAT,
                    .blend = { daxa::BlendInfo {
                        .src_color_blend_factor = daxa::BlendFactor::ZERO,
                        .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_COLOR,
                        .color_blend_op = daxa::BlendOp::ADD,
                        .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .dst_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .alpha_blend_op = daxa::BlendOp::ADD,
                    }},
                },
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = false,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE,
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshletsTransparentPush)),
            .name = std::string{DrawMeshletsTransparentTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_wboit_accumulation_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0, 0.0, 0.0, 0.0}
                },
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_wboit_reveal_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0, 1.0, 1.0, 1.0}
                },
            },
            .depth_attachment = { daxa::RenderAttachmentInfo {
                .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                .load_op = daxa::AttachmentLoadOp::LOAD,
                .store_op = daxa::AttachmentStoreOp::DONT_CARE,
                .clear_value = daxa::DepthValue { .depth = 0.0f, .stencil = {} },
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshletsTransparentTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_mesh_tasks_indirect(daxa::DrawMeshTasksIndirectInfo {
            .indirect_buffer = ti.get(this->AT.u_command).ids[0],
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
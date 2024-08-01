#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DebugDraw)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, u_image)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DebugDrawPush {
    DAXA_TH_BLOB(DebugDraw, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"

struct DebugDrawTask : DebugDraw::Task {
    DebugDraw::Task::AttachmentViews views = {};
    Shaper::Context* context = {};
    DebugDrawPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .vertex_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/debug.glsl" }, },
                .compile_options = {
                    .entry_point = "main",
                    .language = daxa::ShaderLanguage::GLSL,
                    .defines = { { std::string{DebugDrawTask::name()} + "_SHADER", "1" } },
                }
            },
            .fragment_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/debug.glsl" }, },
                .compile_options = { 
                    .entry_point = "main",
                    .language = daxa::ShaderLanguage::GLSL,
                    .defines = { { std::string{DebugDrawTask::name()} + "_SHADER", "1" } } 
                }
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::LINE_STRIP,
                .primitive_restart_enable = {},
                .polygon_mode = daxa::PolygonMode::FILL,
                .face_culling = daxa::FaceCullFlagBits::NONE,
                .front_face_winding = daxa::FrontFaceWinding::CLOCKWISE,
                .depth_clamp_enable = {},
                .rasterizer_discard_enable = {},
                .depth_bias_enable = {},
                .depth_bias_constant_factor = 0.0f,
                .depth_bias_clamp = 0.0f,
                .depth_bias_slope_factor = 0.0f,
                .line_width = 1.7f,
            },
            .push_constant_size = static_cast<u32>(sizeof(DebugDrawPush) + DebugDrawTask::Task::attachment_shader_blob_size()),
            .name = std::string{DebugDrawTask::name()}
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
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(DebugDraw::Task::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = context->debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, aabb_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
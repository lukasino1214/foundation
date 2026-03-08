#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DebugAABBDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_render_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DebugAABBDrawPush {
    DAXA_TH_BLOB(DebugAABBDraw, uses)
    daxa_u32 dummy;
};

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DebugCircleDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_render_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DebugCircleDrawPush {
    DAXA_TH_BLOB(DebugCircleDraw, uses)
    daxa_u32 dummy;
};

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DebugLineDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_render_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DebugLineDrawPush {
    DAXA_TH_BLOB(DebugLineDraw, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/gpu_context.hpp"

struct DebugAABBDrawTask : DebugAABBDraw::Task {
    DebugAABBDrawTask::Task::AttachmentViews views = {};
    foundation::GPUContext* gpu_context = {};
    DebugAABBDrawPush push = {};

    void assign_blob(auto & arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DebugAABBDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "aabb_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugAABBDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugAABBDrawTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R16G16B16A16_UNORM }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::LINE_LIST,
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
            .push_constant_size = s_cast<u32>(sizeof(DebugAABBDrawPush)),
            .name = std::string{DebugAABBDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        u32 size_x = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_render_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .depth_attachment = { 
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = daxa::DepthValue{ .depth = 0.0f, .stencil = {} },
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*gpu_context->raster_pipelines.at(DebugAABBDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = gpu_context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, aabb_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};

struct DebugCircleDrawTask : DebugCircleDraw::Task {
    DebugCircleDrawTask::Task::AttachmentViews views = {};
    foundation::GPUContext* gpu_context = {};
    DebugCircleDrawPush push = {};

    void assign_blob(auto & arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DebugCircleDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "circle_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugCircleDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugCircleDrawTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R16G16B16A16_UNORM }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
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
            .push_constant_size = s_cast<u32>(sizeof(DebugCircleDrawPush)),
            .name = std::string{DebugCircleDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        u32 size_x = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_render_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .depth_attachment = { 
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = daxa::DepthValue{ .depth = 0.0f, .stencil = {} },
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*gpu_context->raster_pipelines.at(DebugCircleDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = gpu_context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, circle_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};

struct DebugLineDrawTask : DebugLineDraw::Task {
    DebugLineDrawTask::Task::AttachmentViews views = {};
    foundation::GPUContext* gpu_context = {};
    DebugLineDrawPush push = {};

    void assign_blob(auto & arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DebugLineDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "line_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugLineDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugLineDrawTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R16G16B16A16_UNORM }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
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
            .push_constant_size = s_cast<u32>(sizeof(DebugLineDrawPush)),
            .name = std::string{DebugLineDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        u32 size_x = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_render_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .depth_attachment = { 
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = daxa::DepthValue{ .depth = 0.0f, .stencil = {} },
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*gpu_context->raster_pipelines.at(DebugLineDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = gpu_context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, line_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};
#endif
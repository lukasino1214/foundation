#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DebugEntityOOBDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_DECL_TASK_HEAD_END

struct DebugEntityOOBDrawPush {
    DAXA_TH_BLOB(DebugEntityOOBDraw, uses)
    daxa_u32 dummy;
};

DAXA_DECL_TASK_HEAD_BEGIN(DebugAABBDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_DECL_TASK_HEAD_END

struct DebugAABBDrawPush {
    DAXA_TH_BLOB(DebugAABBDraw, uses)
    daxa_u32 dummy;
};

DAXA_DECL_TASK_HEAD_BEGIN(DebugCircleDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_DECL_TASK_HEAD_END

struct DebugCircleDrawPush {
    DAXA_TH_BLOB(DebugCircleDraw, uses)
    daxa_u32 dummy;
};

DAXA_DECL_TASK_HEAD_BEGIN(DebugLineDraw)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_IMAGE_TYPED(COLOR_ATTACHMENT, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_DECL_TASK_HEAD_END

struct DebugLineDrawPush {
    DAXA_TH_BLOB(DebugLineDraw, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"

struct DebugEntityOOBDrawTask : DebugEntityOOBDraw::Task {
    DebugEntityOOBDrawTask::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DebugEntityOOBDrawPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DebugEntityOOBDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "entity_obb_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugEntityOOBDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugEntityOOBDrawTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
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
            .push_constant_size = s_cast<u32>(sizeof(DebugEntityOOBDrawPush)),
            .name = std::string{DebugEntityOOBDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.y;

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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DebugEntityOOBDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, entity_oob_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};

struct DebugAABBDrawTask : DebugAABBDraw::Task {
    DebugAABBDrawTask::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DebugAABBDrawPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DebugAABBDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "aabb_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugAABBDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugAABBDrawTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
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
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.y;

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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DebugAABBDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, aabb_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};

struct DebugCircleDrawTask : DebugCircleDraw::Task {
    DebugCircleDrawTask::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DebugCircleDrawPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DebugCircleDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "circle_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugCircleDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugCircleDrawTask::name()} + "_SHADER", "1" } } 
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
            .push_constant_size = s_cast<u32>(sizeof(DebugCircleDrawPush)),
            .name = std::string{DebugCircleDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.y;

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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DebugCircleDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, circle_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};

struct DebugLineDrawTask : DebugLineDraw::Task {
    DebugLineDrawTask::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DebugLineDrawPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DebugLineDraw";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "line_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugLineDrawTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/common/tasks/debug.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DebugLineDrawTask::name()} + "_SHADER", "1" } } 
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
            .push_constant_size = s_cast<u32>(sizeof(DebugLineDrawPush)),
            .name = std::string{DebugLineDrawTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.y;

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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DebugLineDrawTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_indirect(daxa::DrawIndirectInfo { 
            .draw_command_buffer = context->shader_debug_draw_context.buffer,
            .indirect_buffer_offset = offsetof(ShaderDebugBufferHead, line_draw_indirect_info),
            .draw_count = 1,
            .draw_command_stride = sizeof(DrawIndirectStruct),
            .is_indexed = false,
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ExtractDepth)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(READ_WRITE_CONCURRENT, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct ExtractDepthPush {
    DAXA_TH_BLOB(ExtractDepth, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct ExtractDepthTask : ExtractDepth::Task {
    ExtractDepth::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    ExtractDepthPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "ExtractDepth";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/extract_depth.slang" }, },
                .entry_point = "extract_depth_vert",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{ExtractDepthTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/extract_depth.slang" }, },
                .entry_point = "extract_depth_frag",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{ExtractDepthTask::name()} + "_SHADER", "1" } } 
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = s_cast<u32>(sizeof(ExtractDepthPush)),
            .name = std::string{ExtractDepthTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .depth_attachment = { daxa::RenderAttachmentInfo {
                .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue { .depth = 0.0f, .stencil = {} },
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(ExtractDepthTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw(daxa::DrawInfo { .vertex_count = 3 });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
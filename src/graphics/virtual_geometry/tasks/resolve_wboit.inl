#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ResolveWBOIT)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_color_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<daxa_f32vec4>, u_wboit_accumulation_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<daxa_f32>, u_wboit_reveal_image)
DAXA_DECL_TASK_HEAD_END

struct ResolveWBOITPush {
    DAXA_TH_BLOB(ResolveWBOIT, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct ResolveWBOITTask : ResolveWBOIT::Task {
    ResolveWBOIT::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    ResolveWBOITPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "ResolveWBOIT";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/resolve_wboit.slang" }, },
                .entry_point = "resolve_wboit_vert",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{ResolveWBOITTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/resolve_wboit.slang" }, },
                .entry_point = "resolve_wboit_frag",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{ResolveWBOITTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                daxa::RenderAttachment {
                    .format = daxa::Format::R8G8B8A8_UNORM,
                    .blend = { daxa::BlendInfo {
                        .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA,
                        .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                        .color_blend_op = daxa::BlendOp::ADD,
                        .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .dst_alpha_blend_factor = daxa::BlendFactor::ONE,
                        .alpha_blend_op = daxa::BlendOp::ADD,
                    }},
                },
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE,
            },
            .push_constant_size = s_cast<u32>(sizeof(ResolveWBOITPush)),
            .name = std::string{ResolveWBOITTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_color_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_color_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_color_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(ResolveWBOITTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw(daxa::DrawInfo { .vertex_count = 3 });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
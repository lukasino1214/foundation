#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(Triangle)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_image)
DAXA_DECL_TASK_HEAD_END

struct TrianglePush {
    DAXA_TH_BLOB(Triangle, uses)
    u32 dummy;
};

#if __cplusplus
struct TriangleTask : Triangle::Task {
    Triangle::Task::AttachmentViews views = {};
    Shaper::Context* context = {};
    TrianglePush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .vertex_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/traditional/tasks/triangle.slang" }, },
                .compile_options = {
                    .entry_point = "vertex_main",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{TriangleTask::name()} + "_SHADER", "1" } },
                }
            },
            .fragment_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/traditional/tasks/triangle.slang" }, },
                .compile_options = { 
                    .entry_point = "fragment_main",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{TriangleTask::name()} + "_SHADER", "1" } } 
                }
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
            .raster = {
            },
            .push_constant_size = static_cast<u32>(sizeof(TrianglePush) + Triangle::Task::attachment_shader_blob_size()),
            .name = std::string{TriangleTask::name()}
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

        render_cmd.set_pipeline(*context->raster_pipelines.at(Triangle::Task::name()));
        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);
        render_cmd.draw(daxa::DrawInfo { 
            .vertex_count = 3
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
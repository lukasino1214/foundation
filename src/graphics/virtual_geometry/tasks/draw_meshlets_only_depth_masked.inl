#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsOnlyDepthMaskedWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsOnlyDepthMaskedWriteCommandPush {
    DAXA_TH_BLOB(DrawMeshletsOnlyDepthMaskedWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMeshletsOnlyDepthMaskedWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            "DrawMeshletsOnlyDepthMaskedWriteCommand",
                                            DrawMeshletsOnlyDepthMaskedWriteCommand::Task, 
                                            DrawMeshletsOnlyDepthMaskedWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth_masked.slang", 
                                            "draw_meshlets_only_depth_masked_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsOnlyDepthMasked)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsOnlyDepthMaskedPush {
    DAXA_TH_BLOB(DrawMeshletsOnlyDepthMasked, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"

struct DrawMeshletsOnlyDepthMaskedTask : DrawMeshletsOnlyDepthMasked::Task {
    DrawMeshletsOnlyDepthMasked::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DrawMeshletsOnlyDepthMaskedPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return  "DrawMeshletsOnlyDepthMasked";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .mesh_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth_masked.slang" }, },
                .entry_point = "draw_meshlets_only_depth_masked_mesh",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsOnlyDepthMaskedTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth_masked.slang" }, },
                .entry_point = "draw_meshlets_only_depth_masked_frag",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsOnlyDepthMaskedTask::name()} + "_SHADER", "1" } },
            },
            .task_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth_masked.slang" }, },
                .entry_point = "draw_meshlets_only_depth_masked_task",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsOnlyDepthMaskedTask::name()} + "_SHADER", "1" } },
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshletsOnlyDepthMaskedPush)),
            .name = std::string{DrawMeshletsOnlyDepthMaskedTask::name()}
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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshletsOnlyDepthMaskedTask::name()));

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
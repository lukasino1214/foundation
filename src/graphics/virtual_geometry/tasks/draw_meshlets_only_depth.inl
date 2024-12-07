#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsOnlyDepthWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsOnlyDepthWriteCommandPush {
    DAXA_TH_BLOB(DrawMeshletsOnlyDepthWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMeshletsOnlyDepthWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            DrawMeshletsOnlyDepthWriteCommand::Task, 
                                            DrawMeshletsOnlyDepthWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth.slang", 
                                            "draw_meshlets_only_depth_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsOnlyDepth)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsOnlyDepthPush {
    DAXA_TH_BLOB(DrawMeshletsOnlyDepth, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"

struct DrawMeshletsOnlyDepthTask : DrawMeshletsOnlyDepth::Task {
    DrawMeshletsOnlyDepth::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DrawMeshletsOnlyDepthPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .mesh_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth.slang" }, },
                .compile_options = {
                    .entry_point = "draw_meshlets_only_depth_mesh",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{DrawMeshletsOnlyDepthTask::name()} + "_SHADER", "1" } },
                }
            },
            .task_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_only_depth.slang" }, },
                .compile_options = {
                    .entry_point = "draw_meshlets_only_depth_task",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{DrawMeshletsOnlyDepthTask::name()} + "_SHADER", "1" } },
                }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshletsOnlyDepthPush)),
            .name = std::string{DrawMeshletsOnlyDepthTask::name()}
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

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshletsOnlyDepth::Task::name()));

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
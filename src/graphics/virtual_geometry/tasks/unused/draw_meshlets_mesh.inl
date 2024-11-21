#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsMeshWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsMeshWriteCommandPush {
    DAXA_TH_BLOB(DrawMeshletsMeshWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMeshletsMeshWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            DrawMeshletsMeshWriteCommand::Task, 
                                            DrawMeshletsMeshWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/draw_meshlets_mesh.slang", 
                                            "draw_meshlets_mesh_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsMesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(MeshletIndices), u_hw_culled_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ_WRITE_CONCURRENT, daxa_BufferPtr(MeshletIndices), u_sw_culled_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DrawIndirectStruct), u_command)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_SAMPLED, REGULAR_2D, u_hiz)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsMeshPush {
    DAXA_TH_BLOB(DrawMeshletsMesh, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct DrawMeshletsMeshTask : DrawMeshletsMesh::Task {
    DrawMeshletsMesh::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DrawMeshletsMeshPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .mesh_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_mesh.slang" }, },
                .compile_options = {
                    .entry_point = "draw_meshlets_mesh",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{DrawMeshletsMeshTask::name()} + "_SHADER", "1" } },
                }
            },
            .fragment_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_mesh.slang" }, },
                .compile_options = { 
                    .entry_point = "draw_meshlets_frag",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{DrawMeshletsMeshTask::name()} + "_SHADER", "1" } } 
                }
            },
            .task_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets_mesh.slang" }, },
                .compile_options = {
                    .entry_point = "draw_meshlets_task",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{DrawMeshletsMeshTask::name()} + "_SHADER", "1" } },
                }
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshletsMeshPush)),
            .name = std::string{DrawMeshletsMeshTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshletsMesh::Task::name()));

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
#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshletsWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsWriteCommandPush {
    DAXA_TH_BLOB(DrawMeshletsWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMeshletsWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            "DrawMeshletsWriteCommand",
                                            DrawMeshletsWriteCommand::Task, 
                                            DrawMeshletsWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/draw_meshlets.slang", 
                                            "draw_meshlets_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(DrawMeshlets)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(READ_WRITE_CONCURRENT, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_TYPED(READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshletsPush {
    DAXA_TH_BLOB(DrawMeshlets, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct DrawMeshletsTask : DrawMeshlets::Task {
    DrawMeshlets::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    DrawMeshletsPush push = {};

    void assign_blob(auto & arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "DrawMeshlets";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .mesh_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets.slang" }, },
                .entry_point = "draw_meshlets_mesh",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets.slang" }, },
                .entry_point = "draw_meshlets_frag",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTask::name()} + "_SHADER", "1" } } 
            },
            .task_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/draw_meshlets.slang" }, },
                .entry_point = "draw_meshlets_task",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshletsTask::name()} + "_SHADER", "1" } },
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshletsPush)),
            .name = std::string{DrawMeshletsTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshletsTask::name()));

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
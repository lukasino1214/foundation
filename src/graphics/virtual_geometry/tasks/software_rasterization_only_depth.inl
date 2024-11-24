#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepthWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthWriteCommandPush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepthWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationOnlyDepthWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SoftwareRasterizationOnlyDepthWriteCommand::Task, 
                                            SoftwareRasterizationOnlyDepthWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang", 
                                            "software_rasterization_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepth)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_TYPED(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthPush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepth, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"
#include "ecs/entity.hpp"
#include "ecs/components.hpp"
#include "ecs/asset_manager.hpp"

struct SoftwareRasterizationOnlyDepthTask : SoftwareRasterizationOnlyDepth::Task {
    SoftwareRasterizationOnlyDepth::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    SoftwareRasterizationOnlyDepthPush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo {
        return daxa::RasterPipelineCompileInfo {
            .mesh_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang" }, },
                .compile_options = {
                    .entry_point = "software_rasterization_mesh",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{SoftwareRasterizationOnlyDepthTask::name()} + "_SHADER", "1" } },
                }
            },
            .fragment_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang" }, },
                .compile_options = { 
                    .entry_point = "software_rasterization_frag",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{SoftwareRasterizationOnlyDepthTask::name()} + "_SHADER", "1" } } 
                }
            },
            .task_shader_info = daxa::ShaderCompileInfo {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang" }, },
                .compile_options = {
                    .entry_point = "software_rasterization_task",
                    .language = daxa::ShaderLanguage::SLANG,
                    .defines = { { std::string{SoftwareRasterizationOnlyDepthTask::name()} + "_SHADER", "1" } },
                }
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT
            },
            .push_constant_size = s_cast<u32>(sizeof(SoftwareRasterizationOnlyDepthPush)),
            .name = std::string{SoftwareRasterizationOnlyDepthTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_depth_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(SoftwareRasterizationOnlyDepthTask::name()));

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
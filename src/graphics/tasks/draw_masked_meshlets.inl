#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(DrawMaskedMeshletsWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstance), u_culled_meshlet_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(CulledMeshletInstancesRegion), u_culled_meshlet_instances_region)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct DrawMaskedMeshletsWriteCommandPush {
    DAXA_TH_BLOB(DrawMaskedMeshletsWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DrawMaskedMeshletsWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
    "DrawMaskedMeshletsWriteCommand",
    DrawMaskedMeshletsWriteCommand::Task, 
    DrawMaskedMeshletsWriteCommandPush, 
    "src/graphics/tasks/draw_masked_meshlets.slang", 
    "draw_meshlets_write_command"
>;
#endif

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DrawMaskedMeshlets)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(f32mat4x3), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstance), u_culled_meshlet_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(CulledMeshletInstancesRegion), u_culled_meshlet_instances_region)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshInstance), u_mesh_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER(INDIRECT_COMMAND_READ, u_command)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DrawMaskedMeshletsPush {
    DAXA_TH_BLOB(DrawMaskedMeshlets, uses)
};

#if __cplusplus
#include <graphics/gpu_scene.hpp>

using namespace foundation;
struct DrawMaskedMeshletsTask : DrawMaskedMeshlets::Task {
    AttachmentViews views = {};
    GPUContext* gpu_context = {};
    DrawMaskedMeshletsPush push = {};
    GPUScene* gpu_scene = {};

    void assign_blob(auto& arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DrawMaskedMeshlets";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .mesh_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/draw_masked_meshlets.slang" }, },
                .entry_point = "mesh_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMaskedMeshletsTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/draw_masked_meshlets.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMaskedMeshletsTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R32_UINT },
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMaskedMeshletsPush)),
            .name = std::string{DrawMaskedMeshletsTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        u32 size_x = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_visibility_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_visibility_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_u32, 4>{INVALID_ID, INVALID_ID, INVALID_ID, INVALID_ID},
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

        render_cmd.set_pipeline(*gpu_context->raster_pipelines.at(DrawMaskedMeshletsTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);
        render_cmd.push_constant(push);

        render_cmd.draw_mesh_tasks_indirect(daxa::DrawMeshTasksIndirectInfo {
            .indirect_buffer = ti.get(this->AT.u_command).ids[0],
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};
#endif

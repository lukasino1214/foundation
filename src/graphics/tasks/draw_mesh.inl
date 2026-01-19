#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DrawMesh)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_render_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshPush {
    DAXA_TH_BLOB(DrawMesh, uses)
    daxa_BufferPtr(GPUMaterial) materials;
    u32 material_index;
    MeshGeometryData mesh;
    f32mat4x3 matrix;
};

#if __cplusplus
#include "graphics/gpu_context.hpp"
#include "asset/asset_manager.hpp"

using namespace foundation;
struct DrawMeshTask : DrawMesh::Task {
    DrawMeshTask::Task::AttachmentViews views = {};
    GPUContext* context = {};
    DrawMeshPush push = {};
    ModelData* model_data = {};
    Scene* scene = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DrawMesh";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .vertex_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/draw_mesh.slang" }, },
                .entry_point = "vertex_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshTask::name()} + "_SHADER", "1" } },
            },
            .fragment_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/draw_mesh.slang" }, },
                .entry_point = "fragment_main",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{DrawMeshTask::name()} + "_SHADER", "1" } } 
            },
            .color_attachments = {
                { .format = daxa::Format::R8G8B8A8_UNORM }
            },
            .depth_test = { daxa::DepthTestInfo {
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshPush)),
            .name = std::string{DrawMeshTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        u32 size_x = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_render_image).ids[0]).value().size.y;

        auto render_cmd = std::move(ti.recorder).begin_renderpass(daxa::RenderPassBeginInfo {
            .color_attachments = {
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_render_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = std::array<daxa_f32, 4>{0.0f, 0.0f, 0.0f, 1.0},
                }
            },
            .depth_attachment = { 
                daxa::RenderAttachmentInfo {
                    .image_view = ti.get(AT.u_depth_image).ids[0].default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = daxa::DepthValue { .depth = 0.0f, .stencil = {} },
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*context->raster_pipelines.at(DrawMeshTask::name()));

        assign_blob(push.uses, ti.attachment_shader_blob);

        scene->world->each([&](MeshComponent& mesh_component, GlobalMatrix& matrix) {
            for (auto& mesh : model_data->mesh_groups[mesh_component.mesh_group_manifest_entry_index].meshes) {
                push.mesh = mesh.mesh;
                push.materials = context->device.buffer_device_address(model_data->materials_buffer).value();
                push.material_index = mesh.material_index.value_or(INVALID_ID);
                push.matrix = matrix.matrix;

                render_cmd.push_constant(push);
                render_cmd.set_index_buffer({ .id = mesh.index_buffer, .index_type = daxa::IndexType::uint32 });
                render_cmd.push_constant(push);
                render_cmd.draw_indexed({ .index_count = mesh.index_count });
            }
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};
#endif
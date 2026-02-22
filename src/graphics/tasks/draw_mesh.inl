#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DrawMesh)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(f32mat4x3), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshGeometryData), u_mesh_geometry_data)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_TLAS_ID(READ, u_tlas)
DAXA_TH_IMAGE_ID(COLOR_ATTACHMENT, REGULAR_2D, u_render_image)
DAXA_TH_IMAGE_ID(DEPTH_ATTACHMENT, REGULAR_2D, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct DrawMeshPush {
    DAXA_TH_BLOB(DrawMesh, uses)
    u32 mesh_group_index;
    u32 mesh_index;
};

#if __cplusplus
#include <asset/asset_manager.hpp>

using namespace foundation;
struct DrawMeshTask : DrawMesh::Task {
    AttachmentViews views = {};
    GPUContext* gpu_context = {};
    DrawMeshPush push = {};
    AssetManager* asset_manager = {};
    Scene* scene = {};

    void assign_blob(auto& arr, const auto& span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string {
        return "DrawMesh";
    }

    static auto pipeline_config_info() -> daxa::RasterPipelineCompileInfo2 {
        return daxa::RasterPipelineCompileInfo2 {
            .mesh_shader_info = daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderSource { daxa::ShaderFile { .path = "src/graphics/tasks/draw_mesh.slang" }, },
                .entry_point = "mesh_main",
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
                .enable_depth_write = false,
                .depth_test_compare_op = daxa::CompareOp::EQUAL
            } },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = s_cast<u32>(sizeof(DrawMeshPush)),
            .name = std::string{DrawMeshTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        PROFILE_SCOPE_NAMED(draw_mesh_callback);

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
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                }
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        render_cmd.set_pipeline(*gpu_context->raster_pipelines.at(DrawMeshTask::name()));

        scene->world->each([&](MeshComponent& mesh_component) {
            const MeshGroupManifest& mesh_group_manifest = asset_manager->mesh_group_manifests[mesh_component.mesh_group_manifest_entry_index]; 
            const AssetManifest& asset_manifest = asset_manager->asset_manifests[mesh_group_manifest.asset_manifest_index];

            for(u32 mesh_index = 0; mesh_index < mesh_group_manifest.mesh_count; mesh_index++) {
                const MeshManifest& mesh_manifest = asset_manager->mesh_manifests[asset_manifest.mesh_manifest_offset + mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index];
                if(!mesh_manifest.render_info.has_value()) { continue; }

                assign_blob(push.uses, ti.attachment_shader_blob);
                push.mesh_group_index = s_cast<u32>(mesh_component.mesh_group_index.value());
                push.mesh_index = asset_manifest.mesh_manifest_offset + mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index;

                render_cmd.push_constant(push);
                render_cmd.draw_mesh_tasks(mesh_manifest.render_info->meshlet_count, 1, 1);
            }
        });

        ti.recorder = std::move(render_cmd).end_renderpass();
    }
};
#endif
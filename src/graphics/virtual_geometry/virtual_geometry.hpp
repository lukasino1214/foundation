#pragma once

#include <graphics/virtual_geometry/tasks/populate_meshlets.inl>
#include <graphics/virtual_geometry/tasks/cull_meshlets.inl>
#include <graphics/virtual_geometry/tasks/generate_hiz.inl>
#include <graphics/virtual_geometry/tasks/resolve_visibility_buffer.inl>
// #include <graphics/virtual_geometry/tasks/build_index_buffer.inl>
#include <graphics/virtual_geometry/tasks/software_rasterization.inl>
#include <graphics/virtual_geometry/tasks/software_rasterization_only_depth.inl>
#include <graphics/virtual_geometry/tasks/cull_meshes.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_only_depth.inl>
#include <graphics/virtual_geometry/tasks/combine_depth.inl>

#include <pch.hpp>

namespace foundation {
    struct VirtualGeometryTaskInfo {
        Context* context = {};
        daxa::TaskGraph& task_graph;
        daxa::TaskBufferView gpu_scene_data = {};
        daxa::TaskBufferView gpu_entities_data = {};
        daxa::TaskBufferView gpu_meshes = {};
        daxa::TaskBufferView gpu_materials = {};
        daxa::TaskBufferView gpu_transforms = {};
        daxa::TaskBufferView gpu_mesh_groups = {};
        daxa::TaskBufferView gpu_mesh_indices = {};
        daxa::TaskBufferView gpu_meshlet_data = {};
        daxa::TaskBufferView gpu_culled_meshes_data = {};
        daxa::TaskBufferView gpu_hw_culled_meshlet_indices = {};
        daxa::TaskBufferView gpu_sw_culled_meshlet_indices = {};
        daxa::TaskBufferView gpu_readback_material = {};
        daxa::TaskBufferView gpu_readback_mesh = {};
        daxa::TaskImageView color_image = {};
        daxa::TaskImageView depth_image_d32 = {};
        daxa::TaskImageView depth_image_u32 = {};
        daxa::TaskImageView depth_image_f32 = {};
        daxa::TaskImageView visibility_image = {};
    };

    static inline void register_virtual_geometry_gpu_metrics(Context* context) {
        // early pass
        context->gpu_metrics[SoftwareRasterizationOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CombineDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // culling
        context->gpu_metrics[GenerateHizTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[PopulateMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[PopulateMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // late pass
        context->gpu_metrics[DrawMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        // resolve
        context->gpu_metrics[ResolveVisibilityBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
    }

    static inline auto get_virtual_geometry_raster_pipelines() -> std::vector<std::pair<std::string_view, daxa::RasterPipelineCompileInfo>> {
        return {
            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthTask::name(), DrawMeshletsOnlyDepthTask::pipeline_config_info()},
            {SoftwareRasterizationTask::name(), SoftwareRasterizationTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthTask::name(), SoftwareRasterizationOnlyDepthTask::pipeline_config_info()},
        };
    }

    static inline auto get_virtual_geometry_compute_pipelines() -> std::vector<std::pair<std::string_view, daxa::ComputePipelineCompileInfo>> {
        return {
            {PopulateMeshletsWriteCommandTask::name(), PopulateMeshletsWriteCommandTask::pipeline_config_info()},
            {PopulateMeshletsTask::name(), PopulateMeshletsTask::pipeline_config_info()},
            {CullMeshletsWriteCommandTask::name(), CullMeshletsWriteCommandTask::pipeline_config_info()},
            {CullMeshletsTask::name(), CullMeshletsTask::pipeline_config_info()},
            {GenerateHizTask::name(), GenerateHizTask::pipeline_config_info()},
            {ResolveVisibilityBufferTask::name(), ResolveVisibilityBufferTask::pipeline_config_info()},
            // {HWBuildIndexBufferWriteCommandTask::name(), HWBuildIndexBufferWriteCommandTask::pipeline_config_info()},
            // {HWBuildIndexBufferTask::name(), HWBuildIndexBufferTask::pipeline_config_info()},
            // {SWBuildIndexBufferWriteCommandTask::name(), SWBuildIndexBufferWriteCommandTask::pipeline_config_info()},
            // {SWBuildIndexBufferTask::name(), SWBuildIndexBufferTask::pipeline_config_info()},
            {SoftwareRasterizationWriteCommandTask::name(), SoftwareRasterizationWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthWriteCommandTask::name(), SoftwareRasterizationOnlyDepthWriteCommandTask::pipeline_config_info()},
            {CullMeshesWriteCommandTask::name(), CullMeshesWriteCommandTask::pipeline_config_info()},
            {CullMeshesTask::name(), CullMeshesTask::pipeline_config_info()},
            {DrawMeshletsWriteCommandTask::name(), DrawMeshletsWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthWriteCommandTask::name(), DrawMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {CombineDepthTask::name(), CombineDepthTask::pipeline_config_info()},
        };
    }

    static inline void build_virtual_geometry_task_graph(const VirtualGeometryTaskInfo& info) {
        auto u_command = info.task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = s_cast<u32>(glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct))),
            .name = "command",
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.depth_image_d32)},
            .task = [&](daxa::TaskInterface ti) {
                ti.recorder.clear_image({
                    .clear_value = daxa::DepthValue { .depth = 0.0f, .stencil = {} },
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
            },
            .name = "clear depth image d32",
        });

        info.task_graph.add_task(DrawMeshletsOnlyDepthWriteCommandTask {
            .views = std::array{
                DrawMeshletsOnlyDepthWriteCommandTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                DrawMeshletsOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

         info.task_graph.add_task(DrawMeshletsOnlyDepthTask {
            .views = std::array{
                DrawMeshletsOnlyDepthTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                DrawMeshletsOnlyDepthTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                DrawMeshletsOnlyDepthTask::AT.u_meshes | info.gpu_meshes,
                DrawMeshletsOnlyDepthTask::AT.u_transforms | info.gpu_transforms,
                DrawMeshletsOnlyDepthTask::AT.u_materials | info.gpu_materials,
                DrawMeshletsOnlyDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                DrawMeshletsOnlyDepthTask::AT.u_command | u_command,
                DrawMeshletsOnlyDepthTask::AT.u_depth_image | info.depth_image_d32,
            },
            .context = info.context,
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.depth_image_u32)},
            .task = [&](daxa::TaskInterface ti) {
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
            },
            .name = "clear depth image u32",
        });

        info.task_graph.add_task(SoftwareRasterizationOnlyDepthWriteCommandTask {
            .views = std::array{
                SoftwareRasterizationOnlyDepthWriteCommandTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SoftwareRasterizationOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationOnlyDepthTask {
            .views = std::array{
                SoftwareRasterizationOnlyDepthTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SoftwareRasterizationOnlyDepthTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SoftwareRasterizationOnlyDepthTask::AT.u_meshes | info.gpu_meshes,
                SoftwareRasterizationOnlyDepthTask::AT.u_transforms | info.gpu_transforms,
                SoftwareRasterizationOnlyDepthTask::AT.u_materials | info.gpu_materials,
                SoftwareRasterizationOnlyDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                SoftwareRasterizationOnlyDepthTask::AT.u_command | u_command,
                SoftwareRasterizationOnlyDepthTask::AT.u_depth_image | info.depth_image_u32,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CombineDepthTask {
            .views = std::array{
                CombineDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                CombineDepthTask::AT.u_depth_image_d32 | info.depth_image_d32,
                CombineDepthTask::AT.u_depth_image_u32 | info.depth_image_u32,
                CombineDepthTask::AT.u_depth_image_f32 | info.depth_image_f32,
            },
            .context = info.context,
            .dispatch_callback = [info]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(info.context->shader_globals.render_target_size.x, 16),
                    .y = round_up_div(info.context->shader_globals.render_target_size.y, 16),
                    .z = 1
                };
            }
        });

        u32vec2 hiz_size = info.context->shader_globals.next_lower_po2_render_target_size;
        hiz_size = { std::max(hiz_size.x, 1u), std::max(hiz_size.y, 1u) };
        u32 mip_count = std::max(s_cast<u32>(std::ceil(std::log2(std::max(hiz_size.x, hiz_size.y)))), 1u);
        mip_count = std::min(mip_count, u32(GEN_HIZ_LEVELS_PER_DISPATCH));
        auto hiz = info.task_graph.create_transient_image({
            .format = daxa::Format::R32_SFLOAT,
            .size = {hiz_size.x, hiz_size.y, 1},
            .mip_level_count = mip_count,
            .array_layer_count = 1,
            .sample_count = 1,
            .name = "hiz",
        });

        info.task_graph.add_task(GenerateHizTask{
            .views = std::array{
                daxa::attachment_view(GenerateHizTask::AT.u_globals, info.context->shader_globals_buffer),
                daxa::attachment_view(GenerateHizTask::AT.u_src, info.depth_image_f32),
                daxa::attachment_view(GenerateHizTask::AT.u_mips, hiz),
            },
            .context = info.context
        });

        info.task_graph.add_task(CullMeshesWriteCommandTask {
            .views = std::array{
                CullMeshesWriteCommandTask::AT.u_scene_data | info.gpu_scene_data,
                CullMeshesWriteCommandTask::AT.u_command | u_command,
                CullMeshesWriteCommandTask::AT.u_culled_meshes_data | info.gpu_culled_meshes_data,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshesTask {
            .views = std::array{
                CullMeshesTask::AT.u_scene_data | info.gpu_scene_data,
                CullMeshesTask::AT.u_command | u_command,
                CullMeshesTask::AT.u_globals | info.context->shader_globals_buffer,
                CullMeshesTask::AT.u_entities_data | info.gpu_entities_data,
                CullMeshesTask::AT.u_mesh_groups | info.gpu_mesh_groups,
                CullMeshesTask::AT.u_mesh_indices | info.gpu_mesh_indices,
                CullMeshesTask::AT.u_meshes | info.gpu_meshes,
                CullMeshesTask::AT.u_transforms | info.gpu_transforms,
                CullMeshesTask::AT.u_culled_meshes_data | info.gpu_culled_meshes_data,
                CullMeshesTask::AT.u_readback_mesh | info.gpu_readback_mesh,
                CullMeshesTask::AT.u_hiz | hiz,
            },
            .context = info.context,
        });

        info.task_graph.add_task(PopulateMeshletsWriteCommandTask {
            .views = std::array{
                PopulateMeshletsWriteCommandTask::AT.u_culled_meshes_data | info.gpu_culled_meshes_data,
                PopulateMeshletsWriteCommandTask::AT.u_command | u_command,
                PopulateMeshletsWriteCommandTask::AT.u_meshlets_data | info.gpu_meshlet_data,
            },
            .context = info.context,
        });

        info.task_graph.add_task(PopulateMeshletsTask {
            .views = std::array{
                PopulateMeshletsTask::AT.u_culled_meshes_data | info.gpu_culled_meshes_data,
                PopulateMeshletsTask::AT.u_entities_data | info.gpu_entities_data,
                PopulateMeshletsTask::AT.u_mesh_groups | info.gpu_mesh_groups,
                PopulateMeshletsTask::AT.u_mesh_indices | info.gpu_mesh_indices,
                PopulateMeshletsTask::AT.u_meshes | info.gpu_meshes,
                PopulateMeshletsTask::AT.u_command | u_command,
                PopulateMeshletsTask::AT.u_meshlets_data | info.gpu_meshlet_data,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsWriteCommandTask {
            .views = std::array{
                CullMeshletsWriteCommandTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                CullMeshletsWriteCommandTask::AT.u_command | u_command,
                CullMeshletsWriteCommandTask::AT.u_hw_culled_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                CullMeshletsWriteCommandTask::AT.u_sw_culled_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsTask {
            .views = std::array{
                CullMeshletsTask::AT.u_command | u_command,
                CullMeshletsTask::AT.u_globals | info.context->shader_globals_buffer,
                CullMeshletsTask::AT.u_meshes | info.gpu_meshes,
                CullMeshletsTask::AT.u_transforms | info.gpu_transforms,
                CullMeshletsTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                CullMeshletsTask::AT.u_hw_culled_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                CullMeshletsTask::AT.u_sw_culled_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                CullMeshletsTask::AT.u_hiz | hiz,
            },
            .context = info.context,
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.visibility_image)},
            .task = [&](daxa::TaskInterface ti) {
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{INVALID_ID, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
            },
            .name = "clear visibility image",
        });

        info.task_graph.add_task(DrawMeshletsWriteCommandTask {
            .views = std::array{
                DrawMeshletsWriteCommandTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                DrawMeshletsWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

         info.task_graph.add_task(DrawMeshletsTask {
            .views = std::array{
                DrawMeshletsTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                DrawMeshletsTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                DrawMeshletsTask::AT.u_meshes | info.gpu_meshes,
                DrawMeshletsTask::AT.u_transforms | info.gpu_transforms,
                DrawMeshletsTask::AT.u_materials | info.gpu_materials,
                DrawMeshletsTask::AT.u_globals | info.context->shader_globals_buffer,
                DrawMeshletsTask::AT.u_command | u_command,
                DrawMeshletsTask::AT.u_visibility_image | info.visibility_image,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationWriteCommandTask {
            .views = std::array{
                SoftwareRasterizationWriteCommandTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SoftwareRasterizationWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationTask {
            .views = std::array{
                SoftwareRasterizationTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SoftwareRasterizationTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SoftwareRasterizationTask::AT.u_meshes | info.gpu_meshes,
                SoftwareRasterizationTask::AT.u_transforms | info.gpu_transforms,
                SoftwareRasterizationTask::AT.u_materials | info.gpu_materials,
                SoftwareRasterizationTask::AT.u_globals | info.context->shader_globals_buffer,
                SoftwareRasterizationTask::AT.u_command | u_command,
                SoftwareRasterizationTask::AT.u_visibility_image | info.visibility_image,
            },
            .context = info.context,
        });

        info.task_graph.add_task(ResolveVisibilityBufferTask {
            .views = std::array{
                ResolveVisibilityBufferTask::AT.u_globals | info.context->shader_globals_buffer,
                ResolveVisibilityBufferTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                ResolveVisibilityBufferTask::AT.u_meshes | info.gpu_meshes,
                ResolveVisibilityBufferTask::AT.u_transforms | info.gpu_transforms,
                ResolveVisibilityBufferTask::AT.u_materials | info.gpu_materials,
                ResolveVisibilityBufferTask::AT.u_readback_material | info.gpu_readback_material,
                ResolveVisibilityBufferTask::AT.u_visibility_image | info.visibility_image,
                ResolveVisibilityBufferTask::AT.u_image | info.color_image,
            },
            .context = info.context,
            .dispatch_callback = [info]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(info.context->shader_globals.render_target_size.x, 16),
                    .y = round_up_div(info.context->shader_globals.render_target_size.y, 16),
                    .z = 1
                };
            }
        });
    }
}
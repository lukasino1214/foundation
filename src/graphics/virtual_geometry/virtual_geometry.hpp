#pragma once

#include <graphics/virtual_geometry/tasks/populate_meshlets.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets.inl>
#include <graphics/virtual_geometry/tasks/cull_meshlets.inl>
#include <graphics/virtual_geometry/tasks/generate_hiz.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_only_depth.inl>
#include <graphics/virtual_geometry/tasks/resolve_visibility_buffer.inl>
#include <graphics/virtual_geometry/tasks/build_index_buffer.inl>
#include <graphics/virtual_geometry/tasks/software_rasterization.inl>

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
        daxa::TaskBufferView gpu_hw_culled_meshlet_indices = {};
        daxa::TaskBufferView gpu_hw_meshlet_index_buffer = {};
        daxa::TaskBufferView gpu_sw_culled_meshlet_indices = {};
        daxa::TaskBufferView gpu_sw_meshlet_index_buffer = {};
        daxa::TaskBufferView gpu_readback_material = {};
        daxa::TaskImageView color_image = {};
        daxa::TaskImageView depth_image = {};
        daxa::TaskImageView visibility_image = {};
    };

    static inline void register_virtual_geometry_gpu_metrics(Context* context) {
        context->gpu_metrics[PopulateMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[PopulateMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[GenerateHizTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[HWDrawMeshletsOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[HWDrawMeshletsOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SWDrawMeshletsOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SWDrawMeshletsOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[ResolveVisibilityBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[HWBuildIndexBufferWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[HWBuildIndexBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SWBuildIndexBufferWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SWBuildIndexBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
    }

    static inline auto get_virtual_geometry_raster_pipelines() -> std::vector<std::pair<std::string_view, daxa::RasterPipelineCompileInfo>> {
        return {
            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()},
            {HWDrawMeshletsOnlyDepthTask::name(), HWDrawMeshletsOnlyDepthTask::pipeline_config_info()},
            {SWDrawMeshletsOnlyDepthTask::name(), SWDrawMeshletsOnlyDepthTask::pipeline_config_info()},
        };
    }

    static inline auto get_virtual_geometry_compute_pipelines() -> std::vector<std::pair<std::string_view, daxa::ComputePipelineCompileInfo>> {
        return {
            {PopulateMeshletsWriteCommandTask::name(), PopulateMeshletsWriteCommandTask::pipeline_config_info()},
            {PopulateMeshletsTask::name(), PopulateMeshletsTask::pipeline_config_info()},
            {DrawMeshletsWriteCommandTask::name(), DrawMeshletsWriteCommandTask::pipeline_config_info()},
            {CullMeshletsWriteCommandTask::name(), CullMeshletsWriteCommandTask::pipeline_config_info()},
            {CullMeshletsTask::name(), CullMeshletsTask::pipeline_config_info()},
            {GenerateHizTask::name(), GenerateHizTask::pipeline_config_info()},
            {HWDrawMeshletsOnlyDepthWriteCommandTask::name(), HWDrawMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {SWDrawMeshletsOnlyDepthWriteCommandTask::name(), SWDrawMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {ResolveVisibilityBufferTask::name(), ResolveVisibilityBufferTask::pipeline_config_info()},
            {HWBuildIndexBufferWriteCommandTask::name(), HWBuildIndexBufferWriteCommandTask::pipeline_config_info()},
            {HWBuildIndexBufferTask::name(), HWBuildIndexBufferTask::pipeline_config_info()},
            {SWBuildIndexBufferWriteCommandTask::name(), SWBuildIndexBufferWriteCommandTask::pipeline_config_info()},
            {SWBuildIndexBufferTask::name(), SWBuildIndexBufferTask::pipeline_config_info()},
            {SoftwareRasterizationWriteCommandTask::name(), SoftwareRasterizationWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationTask::name(), SoftwareRasterizationTask::pipeline_config_info()},
        };
    }

    static inline void build_virtual_geometry_task_graph(const VirtualGeometryTaskInfo& info) {
        auto u_command = info.task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = s_cast<u32>(glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct))),
            .name = "command",
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.depth_image)},
            .task = [&](daxa::TaskInterface ti) {
                ti.recorder.clear_image({
                    .clear_value = daxa::DepthValue { .depth = 0.0f },
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
            },
            .name = "clear depth image",
        });

        info.task_graph.add_task(HWDrawMeshletsOnlyDepthWriteCommandTask {
            .views = std::array{
                HWDrawMeshletsOnlyDepthWriteCommandTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
                HWDrawMeshletsOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(HWDrawMeshletsOnlyDepthTask {
            .views = std::array{
                HWDrawMeshletsOnlyDepthTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                HWDrawMeshletsOnlyDepthTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
                HWDrawMeshletsOnlyDepthTask::AT.u_meshes | info.gpu_meshes,
                HWDrawMeshletsOnlyDepthTask::AT.u_transforms | info.gpu_transforms,
                HWDrawMeshletsOnlyDepthTask::AT.u_materials | info.gpu_materials,
                HWDrawMeshletsOnlyDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                HWDrawMeshletsOnlyDepthTask::AT.u_command | u_command,
                HWDrawMeshletsOnlyDepthTask::AT.u_depth_image | info.depth_image
            },
            .context = info.context,
        });

        info.task_graph.add_task(SWDrawMeshletsOnlyDepthWriteCommandTask {
            .views = std::array{
                SWDrawMeshletsOnlyDepthWriteCommandTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
                SWDrawMeshletsOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SWDrawMeshletsOnlyDepthTask {
            .views = std::array{
                SWDrawMeshletsOnlyDepthTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SWDrawMeshletsOnlyDepthTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
                SWDrawMeshletsOnlyDepthTask::AT.u_meshes | info.gpu_meshes,
                SWDrawMeshletsOnlyDepthTask::AT.u_transforms | info.gpu_transforms,
                SWDrawMeshletsOnlyDepthTask::AT.u_materials | info.gpu_materials,
                SWDrawMeshletsOnlyDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                SWDrawMeshletsOnlyDepthTask::AT.u_command | u_command,
                SWDrawMeshletsOnlyDepthTask::AT.u_depth_image | info.depth_image
            },
            .context = info.context,
        });

        info.task_graph.add_task(PopulateMeshletsWriteCommandTask {
            .views = std::array{
                PopulateMeshletsWriteCommandTask::AT.u_scene_data | info.gpu_scene_data,
                PopulateMeshletsWriteCommandTask::AT.u_command | u_command,
                PopulateMeshletsWriteCommandTask::AT.u_meshlets_data | info.gpu_meshlet_data,
            },
            .context = info.context,
        });

        info.task_graph.add_task(PopulateMeshletsTask {
            .views = std::array{
                PopulateMeshletsTask::AT.u_scene_data | info.gpu_scene_data,
                PopulateMeshletsTask::AT.u_entities_data | info.gpu_entities_data,
                PopulateMeshletsTask::AT.u_mesh_groups | info.gpu_mesh_groups,
                PopulateMeshletsTask::AT.u_mesh_indices | info.gpu_mesh_indices,
                PopulateMeshletsTask::AT.u_meshes | info.gpu_meshes,
                PopulateMeshletsTask::AT.u_command | u_command,
                PopulateMeshletsTask::AT.u_meshlets_data | info.gpu_meshlet_data,
            },
            .context = info.context,
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
                daxa::attachment_view(GenerateHizTask::AT.u_src, info.depth_image),
                daxa::attachment_view(GenerateHizTask::AT.u_mips, hiz),
            },
            .context = info.context
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

        info.task_graph.add_task(HWBuildIndexBufferWriteCommandTask {
            .views = std::array{
                HWBuildIndexBufferWriteCommandTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                HWBuildIndexBufferWriteCommandTask::AT.u_command | u_command,
                HWBuildIndexBufferWriteCommandTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
            },
            .context = info.context,
        });

        info.task_graph.add_task(HWBuildIndexBufferTask {
            .views = std::array{
                HWBuildIndexBufferTask::AT.u_command | u_command,
                HWBuildIndexBufferTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                HWBuildIndexBufferTask::AT.u_meshlet_indices | info.gpu_hw_culled_meshlet_indices,
                HWBuildIndexBufferTask::AT.u_meshes | info.gpu_meshes,
                HWBuildIndexBufferTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SWBuildIndexBufferWriteCommandTask {
            .views = std::array{
                SWBuildIndexBufferWriteCommandTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SWBuildIndexBufferWriteCommandTask::AT.u_command | u_command,
                SWBuildIndexBufferWriteCommandTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SWBuildIndexBufferTask {
            .views = std::array{
                SWBuildIndexBufferTask::AT.u_command | u_command,
                SWBuildIndexBufferTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SWBuildIndexBufferTask::AT.u_meshlet_indices | info.gpu_sw_culled_meshlet_indices,
                SWBuildIndexBufferTask::AT.u_meshes | info.gpu_meshes,
                SWBuildIndexBufferTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
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
                DrawMeshletsWriteCommandTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
                DrawMeshletsWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(DrawMeshletsTask {
            .views = std::array{
                DrawMeshletsTask::AT.u_index_buffer | info.gpu_hw_meshlet_index_buffer,
                DrawMeshletsTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                DrawMeshletsTask::AT.u_meshes | info.gpu_meshes,
                DrawMeshletsTask::AT.u_transforms | info.gpu_transforms,
                DrawMeshletsTask::AT.u_materials | info.gpu_materials,
                DrawMeshletsTask::AT.u_globals | info.context->shader_globals_buffer,
                DrawMeshletsTask::AT.u_command | u_command,
                DrawMeshletsTask::AT.u_visibility_image | info.visibility_image
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationWriteCommandTask {
            .views = std::array{
                SoftwareRasterizationWriteCommandTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SoftwareRasterizationWriteCommandTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
                SoftwareRasterizationWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationTask {
            .views = std::array{
                SoftwareRasterizationTask::AT.u_index_buffer | info.gpu_sw_meshlet_index_buffer,
                SoftwareRasterizationTask::AT.u_meshlets_data | info.gpu_meshlet_data,
                SoftwareRasterizationTask::AT.u_meshes | info.gpu_meshes,
                SoftwareRasterizationTask::AT.u_transforms | info.gpu_transforms,
                SoftwareRasterizationTask::AT.u_materials | info.gpu_materials,
                SoftwareRasterizationTask::AT.u_globals | info.context->shader_globals_buffer,
                SoftwareRasterizationTask::AT.u_command | u_command,
                SoftwareRasterizationTask::AT.u_visibility_image | info.visibility_image
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
                ResolveVisibilityBufferTask::AT.u_image | info.color_image
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
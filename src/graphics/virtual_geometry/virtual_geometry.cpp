#include "virtual_geometry.hpp"

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

namespace foundation {
    void VirtualGeometryTasks::register_gpu_metrics(Context* context) {
        context->gpu_metrics[Tasks::CLEAR_DEPTH_IMAGE_U32] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[Tasks::CLEAR_VISIBILITY_IMAGE] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // early pass
        context->gpu_metrics[DrawMeshletsOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CombineDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // culling
        context->gpu_metrics[GenerateHizTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // late pass
        context->gpu_metrics[DrawMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        // resolve
        context->gpu_metrics[ResolveVisibilityBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
    
        context->gpu_metrics[SoftwareRasterizationOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
    }

    auto VirtualGeometryTasks::register_performance_metrics() -> std::vector<PerfomanceCategory> {
        std::vector<PerfomanceCategory> performance_categories = {
            PerfomanceCategory {
                .name = "early pass",
                .counters = {
                    {VirtualGeometryTasks::Tasks::CLEAR_DEPTH_IMAGE_U32, "clear depth image u32"},
                    {DrawMeshletsOnlyDepthWriteCommandTask::name(), "hw draw meshlets write"},
                    {DrawMeshletsOnlyDepthTask::name(), "hw draw meshlets"},
                    {SoftwareRasterizationOnlyDepthWriteCommandTask::name(), "sw draw meshlets write"},
                    {SoftwareRasterizationOnlyDepthTask::name(), "sw draw meshlets"},
                    {CombineDepthTask::name(), "combine depth"},
                }
            },
            PerfomanceCategory {
                .name = "culling",
                .counters = {
                    {GenerateHizTask::name(), "generate hiz"},
                    {CullMeshesWriteCommandTask::name(), "cull meshes write"},
                    {CullMeshesTask::name(), "cull meshes"},
                    {CullMeshletsWriteCommandTask::name(), "cull meshlets write"},
                    {CullMeshletsTask::name(), "cull meshlets"},
                }
            },
            PerfomanceCategory {
                .name = "late pass",
                .counters = {
                    {VirtualGeometryTasks::Tasks::CLEAR_VISIBILITY_IMAGE, "clear visibility image"},
                    {DrawMeshletsWriteCommandTask::name(), "hw draw meshlets write"},
                    {DrawMeshletsTask::name(), "hw draw meshlets"},
                    {SoftwareRasterizationWriteCommandTask::name(), "sw draw meshlets write"},
                    {SoftwareRasterizationTask::name(), "sw draw meshlets"},
                }
            },
            PerfomanceCategory {
                .name = "resolve",
                .counters = {
                    {ResolveVisibilityBufferTask::name(), "resolve visibility buffer"}
                }
            },
        };

        return performance_categories;
    }

    auto VirtualGeometryTasks::get_raster_pipelines() -> std::vector<std::pair<std::string_view, daxa::RasterPipelineCompileInfo2>> {
        return {
            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthTask::name(), DrawMeshletsOnlyDepthTask::pipeline_config_info()},
        };
    }

    auto VirtualGeometryTasks::get_compute_pipelines() -> std::vector<std::pair<std::string_view, daxa::ComputePipelineCompileInfo2>> {
        return {
            {CullMeshletsWriteCommandTask::name(), CullMeshletsWriteCommandTask::pipeline_config_info()},
            {CullMeshletsTask::name(), CullMeshletsTask::pipeline_config_info()},
            {GenerateHizTask::name(), GenerateHizTask::pipeline_config_info()},
            {ResolveVisibilityBufferTask::name(), ResolveVisibilityBufferTask::pipeline_config_info()},
            {CullMeshesWriteCommandTask::name(), CullMeshesWriteCommandTask::pipeline_config_info()},
            {CullMeshesTask::name(), CullMeshesTask::pipeline_config_info()},
            {DrawMeshletsWriteCommandTask::name(), DrawMeshletsWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthWriteCommandTask::name(), DrawMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {CombineDepthTask::name(), CombineDepthTask::pipeline_config_info()},
            {SoftwareRasterizationWriteCommandTask::name(), SoftwareRasterizationWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthWriteCommandTask::name(), SoftwareRasterizationOnlyDepthWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationTask::name(), SoftwareRasterizationTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthTask::name(), SoftwareRasterizationOnlyDepthTask::pipeline_config_info()},
        };
    }

    void VirtualGeometryTasks::build_task_graph(const VirtualGeometryTasks::Info& info) {
        auto u_command = info.task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = s_cast<u32>(glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct))),
            .name = "command",
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.overdraw_image)},
            .task = [&](daxa::TaskInterface ti) {
                ti.recorder.clear_image({
                    .clear_value = std::array<i32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
            },
            .name = "clear overdraw image",
        });

        info.task_graph.add_task(DrawMeshletsOnlyDepthWriteCommandTask {
            .views = DrawMeshletsOnlyDepthWriteCommandTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_command = u_command
            },
            .context = info.context,
        });

        info.task_graph.add_task(DrawMeshletsOnlyDepthTask {
            .views = DrawMeshletsOnlyDepthTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_materials = info.gpu_materials,
                .u_globals = info.context->shader_globals_buffer,
                .u_command = u_command,
                .u_depth_image = info.depth_image_d32,
            },
            .context = info.context,
        });

        auto* context = info.context;
        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.depth_image_u32)},
            .task = [context](daxa::TaskInterface ti) {
                context->gpu_metrics[VirtualGeometryTasks::Tasks::CLEAR_DEPTH_IMAGE_U32]->start(ti.recorder);
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
                context->gpu_metrics[VirtualGeometryTasks::Tasks::CLEAR_DEPTH_IMAGE_U32]->end(ti.recorder);
            },
            .name = std::string{VirtualGeometryTasks::Tasks::CLEAR_DEPTH_IMAGE_U32},
        });

        info.task_graph.add_task(SoftwareRasterizationOnlyDepthWriteCommandTask {
            .views = SoftwareRasterizationOnlyDepthWriteCommandTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationOnlyDepthTask {
            .views = SoftwareRasterizationOnlyDepthTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_materials = info.gpu_materials,
                .u_globals = info.context->shader_globals_buffer,
                .u_depth_image = info.depth_image_u32,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CombineDepthTask {
            .views = CombineDepthTask::Views {
                .u_globals = info.context->shader_globals_buffer,
                .u_depth_image_d32 = info.depth_image_d32,
                .u_depth_image_u32 = info.depth_image_u32,
                .u_depth_image_f32 = info.depth_image_f32,
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
            .views = GenerateHizTask::Views {
                .u_globals = info.context->shader_globals_buffer,
                .u_src = info.depth_image_f32,
                .u_mips = hiz,
            },
            .context = info.context
        });

        info.task_graph.add_task(CullMeshesWriteCommandTask {
            .views = CullMeshesWriteCommandTask::Views {
                .u_scene_data = info.gpu_scene_data,
                .u_culled_meshes_data = info.gpu_culled_meshes_data,
                .u_prefix_sum_work_expansion_mesh = info.gpu_prefix_sum_work_expansion_mesh,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshesTask {
            .views = CullMeshesTask::Views {
                .u_scene_data = info.gpu_scene_data,
                .u_globals = info.context->shader_globals_buffer,
                .u_entities_data = info.gpu_entities_data,
                .u_mesh_groups = info.gpu_mesh_groups,
                .u_mesh_indices = info.gpu_mesh_indices,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_culled_meshes_data = info.gpu_culled_meshes_data,
                .u_readback_mesh = info.gpu_readback_mesh,
                .u_prefix_sum_work_expansion_mesh = info.gpu_prefix_sum_work_expansion_mesh,
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsWriteCommandTask {
            .views = CullMeshletsWriteCommandTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_prefix_sum_work_expansion_mesh = info.gpu_prefix_sum_work_expansion_mesh,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsTask {
            .views = CullMeshletsTask::Views {
                .u_globals = info.context->shader_globals_buffer,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_prefix_sum_work_expansion_mesh = info.gpu_prefix_sum_work_expansion_mesh,
                .u_culled_meshes_data = info.gpu_culled_meshes_data,
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, info.visibility_image)},
            .task = [context](daxa::TaskInterface ti) {
                context->gpu_metrics[VirtualGeometryTasks::Tasks::CLEAR_VISIBILITY_IMAGE]->start(ti.recorder);
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{INVALID_ID, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
                context->gpu_metrics[VirtualGeometryTasks::Tasks::CLEAR_VISIBILITY_IMAGE]->end(ti.recorder);
            },
            .name = std::string{VirtualGeometryTasks::Tasks::CLEAR_VISIBILITY_IMAGE},
        });

        info.task_graph.add_task(DrawMeshletsWriteCommandTask {
            .views = DrawMeshletsWriteCommandTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(DrawMeshletsTask {
            .views = DrawMeshletsTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_materials = info.gpu_materials,
                .u_globals = info.context->shader_globals_buffer,
                .u_visibility_image = info.visibility_image,
                .u_overdraw_image = info.overdraw_image,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationWriteCommandTask {
            .views = SoftwareRasterizationWriteCommandTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationTask {
            .views = SoftwareRasterizationTask::Views {
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_materials = info.gpu_materials,
                .u_globals = info.context->shader_globals_buffer,
                .u_visibility_image = info.visibility_image,
                .u_overdraw_image = info.overdraw_image,
                .u_command = u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(ResolveVisibilityBufferTask {
            .views = ResolveVisibilityBufferTask::Views {
                .u_globals = info.context->shader_globals_buffer,
                .u_meshlets_data_merged = info.gpu_meshlets_data_merged,
                .u_meshes = info.gpu_meshes,
                .u_transforms = info.gpu_transforms,
                .u_materials = info.gpu_materials,
                .u_readback_material = info.gpu_readback_material,
                .u_mouse_selection_readback = info.gpu_mouse_selection_readback,
                .u_visibility_image = info.visibility_image,
                .u_overdraw_image = info.overdraw_image,
                .u_image = info.color_image,
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
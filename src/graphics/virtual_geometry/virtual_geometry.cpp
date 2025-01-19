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

    auto VirtualGeometryTasks::get_raster_pipelines() -> std::vector<std::pair<std::string_view, daxa::RasterPipelineCompileInfo>> {
        return {
            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthTask::name(), DrawMeshletsOnlyDepthTask::pipeline_config_info()},
        };
    }

    auto VirtualGeometryTasks::get_compute_pipelines() -> std::vector<std::pair<std::string_view, daxa::ComputePipelineCompileInfo>> {
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
            .views = std::array{
                DrawMeshletsOnlyDepthWriteCommandTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                DrawMeshletsOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(DrawMeshletsOnlyDepthTask {
            .views = std::array{
                DrawMeshletsOnlyDepthTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                DrawMeshletsOnlyDepthTask::AT.u_meshes | info.gpu_meshes,
                DrawMeshletsOnlyDepthTask::AT.u_transforms | info.gpu_transforms,
                DrawMeshletsOnlyDepthTask::AT.u_materials | info.gpu_materials,
                DrawMeshletsOnlyDepthTask::AT.u_globals | info.context->shader_globals_buffer,
                DrawMeshletsOnlyDepthTask::AT.u_command | u_command,
                DrawMeshletsOnlyDepthTask::AT.u_depth_image | info.depth_image_d32,
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
            .views = std::array{
                SoftwareRasterizationOnlyDepthWriteCommandTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                SoftwareRasterizationOnlyDepthWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationOnlyDepthTask {
            .views = std::array{
                SoftwareRasterizationOnlyDepthTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
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
                CullMeshesWriteCommandTask::AT.u_prefix_sum_work_expansion_mesh | info.gpu_prefix_sum_work_expansion_mesh,
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
                CullMeshesTask::AT.u_prefix_sum_work_expansion_mesh | info.gpu_prefix_sum_work_expansion_mesh,
                CullMeshesTask::AT.u_hiz | hiz,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsWriteCommandTask {
            .views = std::array{
                CullMeshletsWriteCommandTask::AT.u_command | u_command,
                CullMeshletsWriteCommandTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                CullMeshletsWriteCommandTask::AT.u_prefix_sum_work_expansion_mesh | info.gpu_prefix_sum_work_expansion_mesh,
            },
            .context = info.context,
        });

        info.task_graph.add_task(CullMeshletsTask {
            .views = std::array{
                CullMeshletsTask::AT.u_command | u_command,
                CullMeshletsTask::AT.u_globals | info.context->shader_globals_buffer,
                CullMeshletsTask::AT.u_meshes | info.gpu_meshes,
                CullMeshletsTask::AT.u_transforms | info.gpu_transforms,
                CullMeshletsTask::AT.u_prefix_sum_work_expansion_mesh | info.gpu_prefix_sum_work_expansion_mesh,
                CullMeshletsTask::AT.u_culled_meshes_data | info.gpu_culled_meshes_data,
                CullMeshletsTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                CullMeshletsTask::AT.u_hiz | hiz,
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
            .views = std::array{
                DrawMeshletsWriteCommandTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                DrawMeshletsWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(DrawMeshletsTask {
            .views = std::array{
                DrawMeshletsTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                DrawMeshletsTask::AT.u_meshes | info.gpu_meshes,
                DrawMeshletsTask::AT.u_transforms | info.gpu_transforms,
                DrawMeshletsTask::AT.u_materials | info.gpu_materials,
                DrawMeshletsTask::AT.u_globals | info.context->shader_globals_buffer,
                DrawMeshletsTask::AT.u_command | u_command,
                DrawMeshletsTask::AT.u_visibility_image | info.visibility_image,
                DrawMeshletsTask::AT.u_overdraw_image | info.overdraw_image,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationWriteCommandTask {
            .views = std::array{
                SoftwareRasterizationWriteCommandTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                SoftwareRasterizationWriteCommandTask::AT.u_command | u_command,
            },
            .context = info.context,
        });

        info.task_graph.add_task(SoftwareRasterizationTask {
            .views = std::array{
                SoftwareRasterizationTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                SoftwareRasterizationTask::AT.u_meshes | info.gpu_meshes,
                SoftwareRasterizationTask::AT.u_transforms | info.gpu_transforms,
                SoftwareRasterizationTask::AT.u_materials | info.gpu_materials,
                SoftwareRasterizationTask::AT.u_globals | info.context->shader_globals_buffer,
                SoftwareRasterizationTask::AT.u_command | u_command,
                SoftwareRasterizationTask::AT.u_visibility_image | info.visibility_image,
                SoftwareRasterizationTask::AT.u_overdraw_image | info.overdraw_image,
            },
            .context = info.context,
        });

        info.task_graph.add_task(ResolveVisibilityBufferTask {
            .views = std::array{
                ResolveVisibilityBufferTask::AT.u_globals | info.context->shader_globals_buffer,
                ResolveVisibilityBufferTask::AT.u_meshlets_data_merged | info.gpu_meshlets_data_merged,
                ResolveVisibilityBufferTask::AT.u_meshes | info.gpu_meshes,
                ResolveVisibilityBufferTask::AT.u_transforms | info.gpu_transforms,
                ResolveVisibilityBufferTask::AT.u_materials | info.gpu_materials,
                ResolveVisibilityBufferTask::AT.u_readback_material | info.gpu_readback_material,
                ResolveVisibilityBufferTask::AT.u_mouse_selection_readback | info.gpu_mouse_selection_readback,
                ResolveVisibilityBufferTask::AT.u_visibility_image | info.visibility_image,
                ResolveVisibilityBufferTask::AT.u_overdraw_image | info.overdraw_image,
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
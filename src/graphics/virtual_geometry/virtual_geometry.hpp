#pragma once

#include <graphics/context.hpp>
#include <utils/performace_metrics.hpp>
#include <pch.hpp>

namespace foundation {
    struct VirtualGeometryTasks {
        struct Tasks {
            static inline constexpr std::string_view CLEAR_DEPTH_IMAGE_U32 = "clear depth image u32";
            static inline constexpr std::string_view CLEAR_VISIBILITY_IMAGE = "clear visibility image";
        };

        struct Info {
            Context* context = {};
            daxa::TaskGraph& task_graph;
            daxa::TaskBufferView gpu_scene_data = {};
            daxa::TaskBufferView gpu_entities_data = {};
            daxa::TaskBufferView gpu_meshes = {};
            daxa::TaskBufferView gpu_materials = {};
            daxa::TaskBufferView gpu_transforms = {};
            daxa::TaskBufferView gpu_mesh_groups = {};
            daxa::TaskBufferView gpu_mesh_indices = {};
            daxa::TaskBufferView gpu_meshlets_instance_data = {};
            daxa::TaskBufferView gpu_meshlets_data_merged_opaque = {};
            daxa::TaskBufferView gpu_meshlets_data_merged_masked = {};
            daxa::TaskBufferView gpu_meshlets_data_merged_transparent = {};
            daxa::TaskBufferView gpu_culled_meshes_data = {};
            daxa::TaskBufferView gpu_readback_material = {};
            daxa::TaskBufferView gpu_readback_mesh = {};
            daxa::TaskBufferView gpu_opaque_prefix_sum_work_expansion_mesh = {};
            daxa::TaskBufferView gpu_masked_prefix_sum_work_expansion_mesh = {};
            daxa::TaskBufferView gpu_transparent_prefix_sum_work_expansion_mesh = {};
            daxa::TaskBufferView gpu_sun_light_buffer = {};
            daxa::TaskBufferView gpu_point_light_buffer = {};
            daxa::TaskBufferView gpu_spot_light_buffer = {};
            daxa::TaskImageView color_image = {};
            daxa::TaskImageView depth_image_d32 = {};
            daxa::TaskImageView depth_image_u32 = {};
            daxa::TaskImageView depth_image_f32 = {};
            daxa::TaskImageView visibility_image = {};
            daxa::TaskImageView overdraw_image = {};
            daxa::TaskImageView wboit_accumulation_image = {};
            daxa::TaskImageView wboit_reveal_image = {};
        };

        static void register_gpu_metrics(Context* context);
        static auto register_performance_metrics() -> std::vector<PerfomanceCategory>;
        static auto get_raster_pipelines() -> std::vector<std::pair<std::string_view, daxa::RasterPipelineCompileInfo2>>;
        static auto get_compute_pipelines() -> std::vector<std::pair<std::string_view, daxa::ComputePipelineCompileInfo2>>;
        static void build_task_graph(const Info& info);
    };
}
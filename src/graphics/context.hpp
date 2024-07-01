#pragma once

#include <pch.hpp>
#include <graphics/window.hpp>
#include <graphics/utils/gpu_metric.hpp>

namespace Shaper {
    struct Context {
        Context(const AppWindow& window);
        ~Context();

        daxa::Instance instance = {};
        daxa::Device device = {};
        daxa::Swapchain swapchain = {};
        daxa::PipelineManager pipeline_manager = {};
        daxa::TransferMemoryPool transient_mem;

        ShaderGlobals shader_globals= {};
        daxa::TaskBuffer shader_globals_buffer = {};

        std::unordered_map<std::string_view, std::shared_ptr<daxa::RasterPipeline>> raster_pipelines = {};
        std::unordered_map<std::string_view, std::shared_ptr<daxa::ComputePipeline>> compute_pipelines = {};

        std::unique_ptr<GPUMetricPool> gpu_metric_pool = {};
        std::unordered_map<std::string_view, std::shared_ptr<GPUMetric>> gpu_metrics = {};

        usize frame_index = 0;
    };
}

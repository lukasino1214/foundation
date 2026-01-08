#pragma once

namespace foundation {
    struct NativeWindow;

    struct RayTracingPipelineInfo {
        std::shared_ptr<daxa::RayTracingPipeline> pipeline = {};
        daxa::RayTracingShaderBindingTable sbt = {};
        daxa::BufferId sbt_buffer_id = {};
    };

    struct GPUContext {
        GPUContext(NativeWindow* window);
        ~GPUContext();

        daxa::Instance instance = {};
        daxa::Device device = {};
        daxa::Swapchain swapchain = {};
        daxa::PipelineManager pipeline_manager = {};

        ShaderGlobals shader_globals = {};
        daxa::TaskBuffer shader_globals_buffer = {};

        ankerl::unordered_dense::map<std::string, std::shared_ptr<daxa::RasterPipeline>> raster_pipelines = {};
        ankerl::unordered_dense::map<std::string, std::shared_ptr<daxa::ComputePipeline>> compute_pipelines = {};
        ankerl::unordered_dense::map<std::string, std::shared_ptr<RayTracingPipelineInfo>> ray_tracing_pipelines = {};
    };
}
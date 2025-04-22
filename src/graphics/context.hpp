#pragma once

#include <pch.hpp>
#include <graphics/window.hpp>
#include <graphics/camera.hpp>
#include <graphics/utils/gpu_metric.hpp>

namespace foundation {
    struct Context;

    template<typename T>
    struct CPUDebugDraws {
        u32 max_draws = {};
        u32 vertices = {};
        std::vector<T> cpu_debug_draws = {};

        void draw(const T& draw) {
            if(cpu_debug_draws.size() < max_draws) {
                cpu_debug_draws.push_back(draw);
            }
        }
    };

    struct ShaderDebugDrawContext {
        CPUDebugDraws<ShaderDebugEntityOOBDraw> entity_oob_draws = { .max_draws = 128'000, .vertices = 24 };
        CPUDebugDraws<ShaderDebugAABBDraw> aabb_draws = { .max_draws = 128'000, .vertices = 24 };
        CPUDebugDraws<ShaderDebugCircleDraw> circle_draws = { .max_draws = 128'000, .vertices = 32 };
        CPUDebugDraws<ShaderDebugLineDraw> line_draws = { .max_draws = 128'000, .vertices = 2 };

        daxa::BufferId buffer = {};
        Context* context = {};

        ShaderDebugDrawContext(Context* _context);
        ~ShaderDebugDrawContext();
        void update_debug_buffer(daxa::Device& device, daxa::CommandRecorder& recorder, daxa::TransferMemoryPool& allocator);
    };

    struct SamplerInfoHasher {
        auto operator()(const daxa::SamplerInfo& info) const -> usize {
            usize hash = 0;
            hash |= (s_cast<usize>(info.magnification_filter) << 0);
            hash |= (s_cast<usize>(info.minification_filter) << 1);
            hash |= (s_cast<usize>(info.mipmap_filter) << 2);
            hash |= (s_cast<usize>(info.reduction_mode) << 3);
            hash |= (s_cast<usize>(info.address_mode_u) << 5);
            hash |= (s_cast<usize>(info.address_mode_v) << 8);
            hash |= (s_cast<usize>(info.address_mode_w) << 11);
            hash |= (s_cast<usize>(std::bit_cast<u32>(info.mip_lod_bias)) << 32);
            hash |= (s_cast<usize>(info.enable_anisotropy) << 14);
            hash |= (s_cast<usize>(s_cast<u32>(info.max_anisotropy)) << 15);
            hash |= (s_cast<usize>(info.enable_compare) << 20);
            hash |= (s_cast<usize>(info.compare_op) << 21);
            hash |= (s_cast<usize>(s_cast<u32>(info.min_lod)) << 24);
            hash |= (s_cast<usize>(s_cast<u32>(info.max_lod == 1000.0f ? 16 : info.max_lod)) << 29);
            hash |= (s_cast<usize>(info.border_color) << 34);
            hash |= (s_cast<usize>(info.enable_unnormalized_coordinates) << 37);
            return hash;
        }
    };

    struct SamplerInfoEquality {
        auto operator()(const daxa::SamplerInfo& lhs, const daxa::SamplerInfo& rhs) const -> bool {
            return  lhs.magnification_filter == rhs.magnification_filter && 
                    lhs.minification_filter == rhs.minification_filter &&
                    lhs.mipmap_filter == rhs.mipmap_filter &&
                    lhs.reduction_mode == rhs.reduction_mode &&
                    lhs.address_mode_u == rhs.address_mode_u &&
                    lhs.address_mode_v == rhs.address_mode_v &&
                    lhs.address_mode_w == rhs.address_mode_w &&
                    lhs.mip_lod_bias == rhs.mip_lod_bias &&
                    lhs.enable_anisotropy == rhs.enable_anisotropy &&
                    lhs.max_anisotropy == rhs.max_anisotropy &&
                    lhs.enable_compare == rhs.enable_compare &&
                    lhs.compare_op == rhs.compare_op &&
                    lhs.min_lod == rhs.min_lod &&
                    lhs.max_lod == rhs.max_lod &&
                    lhs.border_color == rhs.border_color &&
                    lhs.enable_unnormalized_coordinates == rhs.enable_unnormalized_coordinates;
        }
    };

    struct Context {
        Context(const NativeWIndow& window);
        ~Context();
        
        daxa::Instance instance = {};
        daxa::Device device = {};
        daxa::Swapchain swapchain = {};
        daxa::PipelineManager pipeline_manager = {};
        daxa::TransferMemoryPool transient_mem;

        std::unique_ptr<std::mutex> resource_mutex = std::make_unique<std::mutex>();
        usize image_memory_usage = {};
        usize buffer_memory_usage = {};

        ShaderGlobals shader_globals = {};
        daxa::TaskBuffer shader_globals_buffer = {};

        std::unordered_map<std::string_view, std::shared_ptr<daxa::RasterPipeline>> raster_pipelines = {};
        std::unordered_map<std::string_view, std::shared_ptr<daxa::ComputePipeline>> compute_pipelines = {};

        std::unique_ptr<GPUMetricPool> gpu_metric_pool = {};
        std::unordered_map<std::string_view, std::shared_ptr<GPUMetric>> gpu_metrics = {};

        std::unordered_map<daxa::SamplerInfo, daxa::SamplerId, SamplerInfoHasher, SamplerInfoEquality> samplers = {};

        ShaderDebugDrawContext shader_debug_draw_context;

        auto get_sampler(const daxa::SamplerInfo& info) -> daxa::SamplerId;

        auto create_image(const daxa::ImageInfo& info) -> daxa::ImageId;
        void destroy_image(const daxa::ImageId& id);
        void destroy_image_deferred(daxa::CommandRecorder& cmd, const daxa::ImageId& id);

        auto create_buffer(const daxa::BufferInfo& info) -> daxa::BufferId;
        void destroy_buffer(const daxa::BufferId& id);
        void destroy_buffer_deferred(daxa::CommandRecorder& cmd, const daxa::BufferId& id);

        void update_shader_globals(ControlledCamera3D& main_camera, ControlledCamera3D& observer_camera, const glm::uvec2& size);

        usize frame_index = 0;
    };
}

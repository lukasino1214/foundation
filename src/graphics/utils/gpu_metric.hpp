#pragma once

#include <pch.hpp>

namespace foundation {
    struct GPUMetricPool {
        explicit GPUMetricPool(const daxa::Device& _device);
        ~GPUMetricPool();

    private:
        friend struct GPUMetric;

        daxa::Device device = {};
        daxa::TimelineQueryPool timeline_query_pool = {};
        u32 query_count = 0;
        f64 timestamp_period = 0.0;
    };

    struct GPUMetric {
        explicit GPUMetric(GPUMetricPool* _gpu_metric_pool);
        ~GPUMetric();

        void start(daxa::CommandRecorder& cmd_list);
        void end(daxa::CommandRecorder& cmd_list);

        f64 time_elapsed = {};
    private:
        GPUMetricPool* gpu_metric_pool = {};
        u32 index = 0;
    };
}
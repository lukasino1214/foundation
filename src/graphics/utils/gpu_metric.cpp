#include "gpu_metric.hpp"

namespace foundation {
    GPUMetricPool::GPUMetricPool(const daxa::Device& _device) : device{_device}, timeline_query_pool{device.create_timeline_query_pool(daxa::TimelineQueryPoolInfo {
            .query_count = 2048,
            .name = "gpu metric pool"
    })}, timestamp_period(s_cast<f64>(device.properties().limits.timestamp_period)) {}

    GPUMetricPool::~GPUMetricPool() = default;

    GPUMetric::GPUMetric(GPUMetricPool* _gpu_metric_pool) : gpu_metric_pool{_gpu_metric_pool}, index{gpu_metric_pool->query_count} {
        gpu_metric_pool->query_count += 2;
    }

    GPUMetric::~GPUMetric() {
        gpu_metric_pool->query_count -= 2;
    }

    void GPUMetric::start(daxa::CommandRecorder& cmd_list) {
        cmd_list.reset_timestamps(daxa::ResetTimestampsInfo {
            .query_pool = gpu_metric_pool->timeline_query_pool,
            .start_index = index,
            .count = 2
        });

        cmd_list.write_timestamp(daxa::WriteTimestampInfo {
            .query_pool = gpu_metric_pool->timeline_query_pool,
            .pipeline_stage = daxa::PipelineStageFlagBits::TOP_OF_PIPE,
            .query_index = index,
        });
    }

    void GPUMetric::end(daxa::CommandRecorder& cmd_list) {
        cmd_list.write_timestamp(daxa::WriteTimestampInfo {
            .query_pool = gpu_metric_pool->timeline_query_pool,
            .pipeline_stage = daxa::PipelineStageFlagBits::BOTTOM_OF_PIPE,
            .query_index = index + 1,
        });

        std::vector<u64> timestamps = gpu_metric_pool->timeline_query_pool.get_query_results(index, 2);

        if(timestamps[1] == 0 || timestamps[3] == 0) { return; }
        time_elapsed = s_cast<f64>(timestamps[2] - timestamps[0]) / gpu_metric_pool->timestamp_period * 1e-6;
    }
}
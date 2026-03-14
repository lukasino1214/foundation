#include <daxa/daxa.inl>
#include "auto_exposure.inl"

groupshared u32 shared_histogram[LUM_HISTOGRAM_BIN_COUNT];

static const float3 PERCEIVED_LUMINANCE_WEIGHTS = float3(0.2127, 0.7152, 0.0722);
static const f32 MIN_LUMINANCE_THRESHOLD = 2e-10;

[[vk::push_constant]] GenLuminanceHistogramH::AttachmentShaderBlob gen_push;

[numthreads(COMPUTE_HISTOGRAM_WG_X, COMPUTE_HISTOGRAM_WG_Y, 1)]
void gen_histogram(u32vec3 thread_index : SV_DispatchThreadID, u32 local_thread_index : SV_GroupIndex) {
    shared_histogram[local_thread_index % LUM_HISTOGRAM_BIN_COUNT] = 0;
    GroupMemoryBarrierWithGroupSync();

    const u32vec2 render_target_size = gen_push.globals.render_target_size;
    if(all(thread_index.xy < render_target_size)) {
        const float3 color_value = gen_push.color_image.get()[thread_index.xy].rgb;

        const f32 luminance = dot(color_value, PERCEIVED_LUMINANCE_WEIGHTS);
        const f32 luminance_log2 = luminance < MIN_LUMINANCE_THRESHOLD ? gen_push.globals.postprocess_settings.min_luminance_log2 : log2(luminance);
        const f32 remapped_luminance = (luminance_log2 - gen_push.globals.postprocess_settings.min_luminance_log2) * gen_push.globals.postprocess_settings.inv_luminance_log2_range;

        const f32 clamped_luminance = saturate(remapped_luminance);
        const u32 bin_index = u32(clamped_luminance * 255);

        InterlockedAdd(shared_histogram[bin_index], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if(local_thread_index < LUM_HISTOGRAM_BIN_COUNT) {
        InterlockedAdd(gen_push.histogram[local_thread_index], shared_histogram[local_thread_index]);
    }
}

[[vk::push_constant]] GenLuminanceAverageH::AttachmentShaderBlob avg_push;

groupshared u32 sum;

u32 saturating_sub(u32 a, u32 b) {
    return a - min(a, b);
}

f32 compute_exposure(f32 ev) {
    const f32 exposure_bias = (avg_push.globals).postprocess_settings.exposure_bias;
    const f32 calibration = (avg_push.globals).postprocess_settings.calibration;
    const f32 sensor_sensitivity = (avg_push.globals).postprocess_settings.sensor_sensitivity;
    const f32 ev100 = ev + log2(sensor_sensitivity * exposure_bias / calibration);
    return 1.0f / (1.2f * exp2(ev100));
}

[numthreads(LUM_HISTOGRAM_BIN_COUNT, 1, 1)]
void gen_average(u32 local_thread_index : SV_GroupIndex) {
    if(local_thread_index == 0) {
        sum = 0;
    }

    u32 count_for_this_bin  = (avg_push.histogram[local_thread_index]);
    shared_histogram[local_thread_index] = count_for_this_bin ;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for(u32 step = 0; step < u32(log2(f32(LUM_HISTOGRAM_BIN_COUNT))); step++) {
        if (local_thread_index < LUM_HISTOGRAM_BIN_COUNT / 2) {
            const u32 mask   = (1u << step) - 1u;
            const u32 rd_idx = ((local_thread_index >> step) << (step + 1)) + mask;
            const u32 wr_idx = rd_idx + 1 + (local_thread_index & mask);
            shared_histogram[wr_idx] += shared_histogram[rd_idx];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    GroupMemoryBarrierWithGroupSync();

    const f32 outlier_frac_lo = avg_push.globals.postprocess_settings.auto_exposure_histogram_clip_lo;
    const f32 outlier_frac_hi = avg_push.globals.postprocess_settings.auto_exposure_histogram_clip_hi;
    const u32 total_entry_count = shared_histogram[LUM_HISTOGRAM_BIN_COUNT - 1];

    const u32 reject_lo_entry_count = u32(f32(total_entry_count) * outlier_frac_lo);
    const u32 entry_count_to_use = u32(f32(total_entry_count) * (outlier_frac_hi - outlier_frac_lo));
    const u32 reject_hi_entry_count = u32(f32(total_entry_count) * outlier_frac_hi);

    const u32 exclusive_prefix_sum = shared_histogram[local_thread_index] - count_for_this_bin ;
    const u32 left_to_reject = saturating_sub(reject_lo_entry_count, exclusive_prefix_sum);
    const u32 left_to_use = min(entry_count_to_use, saturating_sub(reject_hi_entry_count, exclusive_prefix_sum));
    const u32 count_to_use = min(saturating_sub(count_for_this_bin, left_to_reject), left_to_use);

    const f32 t = (f32(local_thread_index) + 0.5f) / f32(LUM_HISTOGRAM_BIN_COUNT);

    InterlockedAdd(sum, u32(t * f32(count_to_use)));
    GroupMemoryBarrierWithGroupSync();

    if(local_thread_index == 0) {
        const f32 valid_pixel_count = max(f32(entry_count_to_use), 1.0f);
        const f32 weighed_average_log2 = f32(sum) / valid_pixel_count;
        const f32 desired_ev = (weighed_average_log2 * avg_push.globals.postprocess_settings.luminance_log2_range) + (avg_push.globals).postprocess_settings.min_luminance_log2;

        const f32 prev_ev = avg_push.exposure_state.ev;

        const f32 tau = avg_push.globals.postprocess_settings.luminance_adaption_tau;
        const f32 luminance_adapt_factor = 1.0f - exp(-(avg_push.globals).delta_time * tau);
        const f32 adapted_ev = prev_ev + (desired_ev - prev_ev) * luminance_adapt_factor;

        avg_push.exposure_state.ev = adapted_ev;
        avg_push.exposure_state.exposure = compute_exposure(adapted_ev);
    }
}

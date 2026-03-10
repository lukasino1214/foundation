#include <daxa/daxa.inl>
#include "bloom.inl"

[[vk::push_constant]] ThresholdImagePush threshold_push;
[numthreads(16, 8, 1)]
void threshold_image(u32vec2 index : SV_DispatchThreadID) {
    if(any(greaterThanEqual(index, threshold_push.size))) { return; }

    const f32vec3 color = threshold_push.uses.u_render_image.get()[index].rgb;
    const f32vec3 bloom = max(f32vec3(0.0f), color - f32vec3(1.0f));
    threshold_push.uses.u_bloom_image.get()[index].rgb += bloom;
}

static const f32vec2 BloomCoords[13] = {
    f32vec2(-1.0f, +1.0f), f32vec2(1.0f, +1.0f),
    f32vec2(-1.0f, -1.0f), f32vec2(1.0f, -1.0f),

    f32vec2(-2.0f, +2.0f), f32vec2(+0.0f, +2.0f), f32vec2(2.0f, +2.0f),
    f32vec2(-2.0f, +0.0f), f32vec2(+0.0f, +0.0f), f32vec2(2.0f, +0.0f),
    f32vec2(-2.0f, -2.0f), f32vec2(+0.0f, -2.0f), f32vec2(2.0f, -2.0f),
};

static const f32 BloomWeights[13] = {
    0.125f, 0.125f,
    0.125f, 0.125f,

    0.03125f, 0.0625f, 0.03125f,
    0.0625f,  0.125f,  0.0625f,
    0.03125f, 0.0625f, 0.03125f,
};

f32vec3 safe_hdr(f32vec3 color) {
    return min(color, f32vec3(65000.0f));
}

f32 max_brightness(f32vec3 color) {
    return max(max(color.r, color.g), color.b);
}

f32vec3 karis_average_block(f32vec3 s1, f32vec3 s2, f32vec3 s3, f32vec3 s4) {
    const f32vec4 brightness = f32vec4(
        max_brightness(s1),
        max_brightness(s2),
        max_brightness(s3),
        max_brightness(s4)
    );

    const f32vec4 weights = 1.0f / (brightness + 1.0f);
    const f32 weight_sum = weights.x + weights.y + weights.z + weights.w;

    return (s1 * weights.x + s2 * weights.y + s3 * weights.z + s4 * weights.w) * (1.0f / weight_sum);
}

f32vec3 downsample_anti_flicker(f32vec2 uv, f32vec2 pixel_size, SamplerState samp, Texture2D<f32vec4> src) {
    f32vec3 samples[13];
    for(u32 i = 0; i < 13; i++) {
        const f32vec2 sample_uv = uv + BloomCoords[i] * pixel_size;
        samples[i] = src.SampleLevel(samp, sample_uv, 0).rgb;
    }

    const f32vec3 center       = karis_average_block(samples[0], samples[1], samples[2], samples[3]);
    const f32vec3 top_left     = karis_average_block(samples[4], samples[5], samples[8], samples[7]);
    const f32vec3 top_right    = karis_average_block(samples[5], samples[6], samples[9], samples[8]);
    const f32vec3 bottom_left  = karis_average_block(samples[7], samples[8], samples[11], samples[10]);
    const f32vec3 bottom_right = karis_average_block(samples[8], samples[9], samples[12], samples[11]);

    return 0.5f * center + 0.125f * (top_left + top_right + bottom_left + bottom_right);;
}

f32vec3 downsample_standard(f32vec2 uv, f32vec2 pixel_size, SamplerState samp, Texture2D<f32vec4> src) {
    f32vec3 result = f32vec3(0.0f);
    for(u32 i = 0; i < 13; i++) {
        const f32vec2 sample_uv = uv + BloomCoords[i] * pixel_size;
        result += BloomWeights[i] * src.SampleLevel(samp, sample_uv, 0).rgb;
    }
    return result;
}

[[vk::push_constant]] BloomDownsamplePush downsample_push;
[numthreads(16, 8, 1)]
void bloom_downsample(u32vec2 index : SV_DispatchThreadID) {
    if(any(greaterThanEqual(index, downsample_push.dst_size))) { return; }

    const f32vec2 uv = (f32vec2(index) + 0.5f) / f32vec2(downsample_push.dst_size);
    const f32vec2 pixel_size = 1.0f / f32vec2(downsample_push.src_size);

    SamplerState samp = SamplerState::get(downsample_push.uses.u_globals->samplers.linear_clamp);
    Texture2D<f32vec4> src = downsample_push.uses.u_src_image.get();

    f32vec3 color;
    if(downsample_push.use_anti_flicker != 0) {
        color = downsample_anti_flicker(uv, pixel_size, samp, src);
    } else {
        color = downsample_standard(uv, pixel_size, samp, src);
    }

    color = safe_hdr(color);
    downsample_push.uses.u_dst_image.get()[index] = f32vec4(color, 1.0f);
}

static const f32vec2 UpsampleCoords[9] = {
    f32vec2(-1.0f, +1.0f), f32vec2(+0.0f, +1.0f), f32vec2(+1.0f, +1.0f),
    f32vec2(-1.0f, +0.0f), f32vec2(+0.0f, +0.0f), f32vec2(+1.0f, +0.0f),
    f32vec2(-1.0f, -1.0f), f32vec2(+0.0f, -1.0f), f32vec2(+1.0f, -1.0f),
};

static const f32 UpsampleWeights[9] = {
    0.0625, 0.125, 0.0625,
    0.125,  0.25,  0.125,
    0.0625, 0.125, 0.0625,
};

[[vk::push_constant]] BloomUpsamplePush upsample_push;
[numthreads(16, 8, 1)]
void bloom_upsample(u32vec2 index : SV_DispatchThreadID) {
    if(any(greaterThanEqual(index, upsample_push.dst_size))) { return; }

    const f32vec2 uv = (f32vec2(index) + 0.5f) / f32vec2(upsample_push.dst_size);
    const f32vec2 pixel_size = 1.0f / f32vec2(upsample_push.dst_size);

    SamplerState samp = SamplerState::get(upsample_push.uses.u_globals->samplers.linear_clamp);
    const f32vec3 current_color = upsample_push.uses.u_downsample_image.get().SampleLevel(samp, uv, 0).rgb;

    f32vec3 upsampled = f32vec3(0.0f);
    Texture2D<f32vec4> src = upsample_push.uses.u_src_image.get();
    for(u32 i = 0; i < 9; i++) {
        f32vec2 sample_uv = uv + UpsampleCoords[i] * pixel_size;
        upsampled += UpsampleWeights[i] * src.SampleLevel(samp, sample_uv, 0).rgb;
    }

    f32vec3 color = lerp(current_color, upsampled, upsample_push.internal_blend);
    color = safe_hdr(color);

    upsample_push.uses.u_dst_image.get()[index] = f32vec4(color, 1.0f);
}

[[vk::push_constant]] ApplyBloomPush apply_push;
[numthreads(16, 8, 1)]
void apply_bloom(u32vec2 index : SV_DispatchThreadID) {
    if(any(greaterThanEqual(index, apply_push.size))) { return; }

    const f32vec2 uv = (f32vec2(index) + 0.5f) / f32vec2(apply_push.size);
    const f32vec3 exposed_color = apply_push.uses.u_render_image.get()[index].rgb;
    const f32vec3 bloom_color = apply_push.uses.u_bloom_image.get().SampleLevel(SamplerState::get(apply_push.uses.u_globals->samplers.linear_clamp), uv, 0).rgb;
    
    apply_push.uses.u_render_image.get()[index].rgb = exposed_color + bloom_color;
}
#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(ThresholdImage)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_render_image)
DAXA_TH_IMAGE_TYPED(READ_WRITE, daxa::RWTexture2DId<f32vec4>, u_bloom_image)
DAXA_DECL_TASK_HEAD_END

struct ThresholdImagePush {
    DAXA_TH_BLOB(ThresholdImage, uses)
    u32vec2 size;
};

#if __cplusplus
using ThresholdImageTask = foundation::ComputeDispatchTask<
    "ThresholdImage",
    ThresholdImage::Task,
    ThresholdImagePush,
    "src/graphics/tasks/bloom.slang",
    "threshold_image">;
#endif

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(BloomDownsample)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_src_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_dst_image)
DAXA_DECL_TASK_HEAD_END

struct BloomDownsamplePush {
    DAXA_TH_BLOB(BloomDownsample, uses)
    u32vec2 src_size;
    u32vec2 dst_size;
    u32 use_anti_flicker;
};

#if __cplusplus
using BloomDownsampleTask = foundation::ComputeDispatchTask<
    "BloomDownsample",
    BloomDownsample::Task,
    BloomDownsamplePush,
    "src/graphics/tasks/bloom.slang",
    "bloom_downsample">;
#endif

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(BloomUpsample)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_src_image)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_downsample_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_dst_image)
DAXA_DECL_TASK_HEAD_END

struct BloomUpsamplePush {
    DAXA_TH_BLOB(BloomUpsample, uses)
    u32vec2 dst_size;
    f32 internal_blend;
};

#if __cplusplus
using BloomUpsampleTask = foundation::ComputeDispatchTask<
    "BloomUpsample",
    BloomUpsample::Task,
    BloomUpsamplePush,
    "src/graphics/tasks/bloom.slang",
    "bloom_upsample">;
#endif

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(ApplyBloom)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_bloom_image)
DAXA_TH_IMAGE_TYPED(READ_WRITE, daxa::RWTexture2DId<f32vec4>, u_render_image)
DAXA_DECL_TASK_HEAD_END

struct ApplyBloomPush {
    DAXA_TH_BLOB(ApplyBloom, uses)
    u32vec2 size;
};

#if __cplusplus
using ApplyBloomTask = foundation::ComputeDispatchTask<
    "ApplyBloom",
    ApplyBloom::Task,
    ApplyBloomPush,
    "src/graphics/tasks/bloom.slang",
    "apply_bloom">;
#endif
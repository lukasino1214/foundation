#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CombineDepth)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(COMPUTE_SHADER_STORAGE_READ_ONLY, daxa::RWTexture2DId<f32>, u_depth_image_d32)
DAXA_TH_IMAGE_TYPED(COMPUTE_SHADER_STORAGE_READ_ONLY, daxa::RWTexture2DId<u32>, u_depth_image_u32)
DAXA_TH_IMAGE_TYPED(COMPUTE_SHADER_STORAGE_WRITE_ONLY, daxa::RWTexture2DId<f32>, u_depth_image_f32)
DAXA_DECL_TASK_HEAD_END

struct CombineDepthPush {
    DAXA_TH_BLOB(CombineDepth, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CombineDepthTask = foundation::ComputeDispatchTask<CombineDepth::Task, CombineDepthPush, "src/graphics/virtual_geometry/tasks/combine_depth.slang", "combine_depth">;
#endif
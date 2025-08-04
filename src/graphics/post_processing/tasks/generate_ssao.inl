#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(GenerateSSAO)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_normal_detail_half_res_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<f32>, u_depth_half_res_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32>, u_ssao_image)
DAXA_DECL_TASK_HEAD_END

struct GenerateSSAOPush {
    DAXA_TH_BLOB(GenerateSSAO, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using GenerateSSAOTask = foundation::ComputeDispatchTask<"GenerateSSAO", GenerateSSAO::Task, GenerateSSAOPush, "src/graphics/post_processing/tasks/generate_ssao.slang", "generate_ssao">;
#endif
#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(DebugImage)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<f32>, u_f32_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_u32_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_color_image)
DAXA_DECL_TASK_HEAD_END

struct DebugImagePush {
    DAXA_TH_BLOB(DebugImage, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using DebugImageTask = foundation::ComputeDispatchTask<"DebugImage", DebugImage::Task, DebugImagePush, "src/graphics/common/tasks/debug_image.slang", "debug_image">;
#endif
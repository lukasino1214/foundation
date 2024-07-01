#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shared/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ClearImage)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, u_image)
DAXA_DECL_TASK_HEAD_END

struct ClearImagePush {
    DAXA_TH_BLOB(ClearImage, uses)
    f32vec3 color;
};

#if __cplusplus
static constexpr inline char const CLEAR_IMAGE_SHADER_PATH[] = "src/graphics/common/tasks/clear_image.slang";
using ClearImageTask = Shaper::ComputeDispatchTask<ClearImage::Task, ClearImagePush, CLEAR_IMAGE_SHADER_PATH>;
#endif
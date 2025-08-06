#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CalculateFrustums)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(TileFrustum), u_tile_frustums)
DAXA_DECL_TASK_HEAD_END

struct CalculateFrustumsPush {
    DAXA_TH_BLOB(CalculateFrustums, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CalculateFrustumsTask = foundation::ComputeDispatchTask<"CalculateFrustums", CalculateFrustums::Task, CalculateFrustumsPush, "src/graphics/post_processing/tasks/calculate_frustums.slang", "calculate_frustums">;
#endif
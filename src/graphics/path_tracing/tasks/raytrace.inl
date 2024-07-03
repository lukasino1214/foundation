#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(RayTrace)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, u_image)
DAXA_DECL_TASK_HEAD_END

struct RayTracePush {
    DAXA_TH_BLOB(RayTrace, uses)
    daxa_u32 dummy;
};

#if __cplusplus
static constexpr inline char const RAY_TRACE_SHADER_PATH[] = "src/graphics/path_tracing/tasks/raytrace.slang";
using RayTraceTask = Shaper::ComputeDispatchTask<RayTrace::Task, RayTracePush, RAY_TRACE_SHADER_PATH>;
#endif
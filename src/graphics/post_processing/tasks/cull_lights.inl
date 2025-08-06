#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CullLights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TileFrustum), u_tile_frustums)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(TileData), u_tile_data)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<f32>, u_depth_image)
DAXA_TH_IMAGE_TYPED(READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_DECL_TASK_HEAD_END

struct CullLightsPush {
    DAXA_TH_BLOB(CullLights, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullLightsTask = foundation::ComputeDispatchTask<"CullLights", CullLights::Task, CullLightsPush, "src/graphics/post_processing/tasks/cull_lights.slang", "cull_lights">;
#endif
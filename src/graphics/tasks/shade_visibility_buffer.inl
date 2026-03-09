#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(ShadeVisibilityBuffer)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(f32mat4x3), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstance), u_culled_meshlet_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshInstance), u_mesh_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_BUFFER(INDIRECT_COMMAND_READ, u_command)
DAXA_TH_TLAS_ID(READ, u_tlas)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_visibility_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_render_image)
DAXA_DECL_TASK_HEAD_END

struct ShadeVisibilityBufferPush {
    DAXA_TH_BLOB(ShadeVisibilityBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using ShadeVisibilityBufferTask = foundation::ComputeDispatchTask<
    "ShadeVisibilityBuffer", 
    ShadeVisibilityBuffer::Task, 
    ShadeVisibilityBufferPush, 
    "src/graphics/tasks/shade_visibility_buffer.slang", 
    "shade_visibility_buffer"
>;
#endif
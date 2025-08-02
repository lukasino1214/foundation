#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ResolveVisibilityBuffer)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstanceData), u_meshlets_instance_data)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SunLight), u_sun)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(u32), u_readback_material)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(MouseSelectionReadback), u_mouse_selection_readback)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_DECL_TASK_HEAD_END

struct ResolveVisibilityBufferPush {
    DAXA_TH_BLOB(ResolveVisibilityBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using ResolveVisibilityBufferTask = foundation::ComputeDispatchTask<"ResolveVisibilityBuffer", ResolveVisibilityBuffer::Task, ResolveVisibilityBufferPush, "src/graphics/virtual_geometry/tasks/resolve_visibility_buffer.slang", "resolve_visibility_buffer">;
#endif
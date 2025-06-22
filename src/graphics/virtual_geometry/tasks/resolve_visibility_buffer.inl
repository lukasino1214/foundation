#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ResolveVisibilityBuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(SunLight), u_sun)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PointLightsData), u_point_lights)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(SpotLightsData), u_spot_lights)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(u32), u_readback_material)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MouseSelectionReadback), u_mouse_selection_readback)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_TYPED(COMPUTE_SHADER_STORAGE_READ_ONLY, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_TH_IMAGE_TYPED(COMPUTE_SHADER_STORAGE_WRITE_ONLY, daxa::RWTexture2DId<f32vec4>, u_image)
DAXA_DECL_TASK_HEAD_END

struct ResolveVisibilityBufferPush {
    DAXA_TH_BLOB(ResolveVisibilityBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using ResolveVisibilityBufferTask = foundation::ComputeDispatchTask<ResolveVisibilityBuffer::Task, ResolveVisibilityBufferPush, "src/graphics/virtual_geometry/tasks/resolve_visibility_buffer.slang", "resolve_visibility_buffer">;
#endif
#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(ResolveVisibilityBuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(u32), u_readback_material)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_STORAGE_READ_ONLY, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_STORAGE_WRITE_ONLY, REGULAR_2D, u_image)
DAXA_DECL_TASK_HEAD_END

struct ResolveVisibilityBufferPush {
    DAXA_TH_BLOB(ResolveVisibilityBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
static constexpr inline char const RESOLVE_VISIBILITY_BUFFER_SHADER_PATH[] = "src/graphics/virtual_geometry/tasks/resolve_visibility_buffer.slang";
using ResolveVisibilityBufferTask = foundation::ComputeDispatchTask<ResolveVisibilityBuffer::Task, ResolveVisibilityBufferPush, RESOLVE_VISIBILITY_BUFFER_SHADER_PATH>;
#endif
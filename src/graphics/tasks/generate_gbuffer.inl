#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(GenerateGBuffer)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(f32mat4x3), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstance), u_culled_meshlet_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshInstance), u_mesh_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_visibility_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<u32>, u_normal_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32vec4>, u_bloom_image)
DAXA_DECL_TASK_HEAD_END

struct GenerateGBufferPush {
    DAXA_TH_BLOB(GenerateGBuffer, uses)
    u32vec2 size;
};

#if __cplusplus
using GenerateGBufferTask = foundation::ComputeDispatchTask<
    "GenerateGBuffer", 
    GenerateGBuffer::Task, 
    GenerateGBufferPush, 
    "src/graphics/tasks/generate_gbuffer.slang", 
    "generate_gbuffer"
>;
#endif
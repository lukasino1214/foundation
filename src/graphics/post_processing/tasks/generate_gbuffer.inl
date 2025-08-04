#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(GenerateGBuffer)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshletInstanceData), u_meshlets_instance_data)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_IMAGE_ID(READ, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<u32>, u_normal_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<u32>, u_normal_half_res_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<u32>, u_normal_detail_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<u32>, u_normal_detail_half_res_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32>, u_depth_half_res_image)
DAXA_DECL_TASK_HEAD_END

struct GenerateGBufferPush {
    DAXA_TH_BLOB(GenerateGBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using GenerateGBufferTask = foundation::ComputeDispatchTask<"GenerateGBuffer", GenerateGBuffer::Task, GenerateGBufferPush, "src/graphics/post_processing/tasks/generate_gbuffer.slang", "generate_gbuffer">;
#endif
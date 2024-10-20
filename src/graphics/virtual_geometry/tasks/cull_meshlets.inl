#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndices), u_hw_culled_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndices), u_sw_culled_meshlet_indices)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsWriteCommandPush {
    DAXA_TH_BLOB(CullMeshletsWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletsWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            CullMeshletsWriteCommand::Task, 
                                            CullMeshletsWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshlets)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndices), u_hw_culled_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndices), u_sw_culled_meshlet_indices)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_SAMPLED, REGULAR_2D, u_hiz)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsPush {
    DAXA_TH_BLOB(CullMeshlets, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletsTask = foundation::IndirectComputeDispatchTask<
                                            CullMeshlets::Task, 
                                            CullMeshletsPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets">;
#endif
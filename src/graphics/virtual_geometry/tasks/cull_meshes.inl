#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshesWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_group_count)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_opaque_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_masked_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_transparent_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshesWriteCommandPush {
    DAXA_TH_BLOB(CullMeshesWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshesWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            "CullMeshesWriteCommand",
                                            CullMeshesWriteCommand::Task, 
                                            CullMeshesWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshes.slang", 
                                            "cull_meshes_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_group_count)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshGroup), u_mesh_groups)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_indices)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(u32), u_readback_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_opaque_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_masked_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_transparent_prefix_sum_work_expansion_mesh)
DAXA_TH_IMAGE_ID(SAMPLED, REGULAR_2D, u_hiz)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshesPush {
    DAXA_TH_BLOB(CullMeshes, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshesTask = foundation::IndirectComputeDispatchTask<
                                            "CullMeshes",
                                            CullMeshes::Task, 
                                            CullMeshesPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshes.slang", 
                                            "cull_meshes">;
#endif
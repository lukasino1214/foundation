#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(CullMeshInstancesWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_group_instance_count)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(CulledMeshInstancesData), u_culled_mesh_instances_data)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshInstancesWriteCommandPush {
    DAXA_TH_BLOB(CullMeshInstancesWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshInstancesWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
    "CullMeshInstancesWriteCommand",
    CullMeshInstancesWriteCommand::Task, 
    CullMeshInstancesWriteCommandPush, 
    "src/graphics/tasks/cull_mesh_instances.slang", 
    "cull_mesh_instances_write_command"
>;
#endif

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(CullMeshInstances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_group_instance_count)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshGroupInstance), u_mesh_group_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_mesh_instance_indices)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshInstance), u_mesh_instances)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(CulledMeshInstancesData), u_culled_mesh_instances_data)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER(INDIRECT_COMMAND_READ, u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshInstancesPush {
    DAXA_TH_BLOB(CullMeshInstances, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshInstancesTask = foundation::IndirectComputeDispatchTask<
    "CullMeshInstances",
    CullMeshInstances::Task, 
    CullMeshInstancesPush, 
    "src/graphics/tasks/cull_mesh_instances.slang", 
    "cull_mesh_instances"
>;
#endif

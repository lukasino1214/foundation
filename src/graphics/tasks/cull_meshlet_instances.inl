#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(CullMeshletInstancesWriteCommand)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(CulledMeshletInstancesData), u_culled_meshlet_instances_data)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletInstancesWriteCommandPush {
    DAXA_TH_BLOB(CullMeshletInstancesWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletInstancesWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
    "CullMeshletInstancesWriteCommand",
    CullMeshletInstancesWriteCommand::Task, 
    CullMeshletInstancesWriteCommandPush, 
    "src/graphics/tasks/cull_meshlet_instances.slang", 
    "cull_meshlet_instances_write_command"
>;
#endif

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(CullMeshletInstances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(MeshInstance), u_mesh_instances)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(f32mat4x3), u_transforms)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(CulledMeshInstancesData), u_culled_mesh_instances_data)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(CulledMeshletInstancesData), u_culled_meshlet_instances_data)
DAXA_TH_BUFFER(INDIRECT_COMMAND_READ, u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletInstancesPush {
    DAXA_TH_BLOB(CullMeshletInstances, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletInstancesTask = foundation::IndirectComputeDispatchTask<
    "CullMeshletInstances",
    CullMeshletInstances::Task, 
    CullMeshletInstancesPush, 
    "src/graphics/tasks/cull_meshlet_instances.slang", 
    "cull_meshlet_instances"
>;
#endif

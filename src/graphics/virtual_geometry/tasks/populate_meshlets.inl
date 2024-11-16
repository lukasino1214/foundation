#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(PopulateMeshletsWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_DECL_TASK_HEAD_END

struct PopulateMeshletsWriteCommandPush {
    DAXA_TH_BLOB(PopulateMeshletsWriteCommand, uses)
    daxa_u32 dummy;
};

DAXA_DECL_TASK_HEAD_BEGIN(PopulateMeshlets)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(EntityData), u_entities_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshGroup), u_mesh_groups)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(u32), u_mesh_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_DECL_TASK_HEAD_END

struct PopulateMeshletsPush {
    DAXA_TH_BLOB(PopulateMeshlets, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using PopulateMeshletsWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            PopulateMeshletsWriteCommand::Task, 
                                            PopulateMeshletsWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/populate_meshlets.slang", 
                                            "populate_meshlets_write_command">;

using PopulateMeshletsTask = foundation::IndirectComputeDispatchTask<
                                            PopulateMeshlets::Task, 
                                            PopulateMeshletsPush, 
                                            "src/graphics/virtual_geometry/tasks/populate_meshlets.slang", 
                                            "populate_meshlets">;
#endif
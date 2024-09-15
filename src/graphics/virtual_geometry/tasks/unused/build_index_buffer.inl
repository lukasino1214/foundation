#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(BuildIndexBufferWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct BuildIndexBufferWriteCommandPush {
    DAXA_TH_BLOB(BuildIndexBufferWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using BuidIndexBufferWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            BuildIndexBufferWriteCommand::Task, 
                                            BuildIndexBufferWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(BuildIndexBuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_DECL_TASK_HEAD_END

struct BuildIndexBufferPush {
    DAXA_TH_BLOB(BuildIndexBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using BuildIndexBufferTask = foundation::IndirectComputeDispatchTask<
                                            BuildIndexBuffer::Task, 
                                            BuildIndexBufferPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer">;
#endif
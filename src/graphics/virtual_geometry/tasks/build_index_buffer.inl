#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(HWBuildIndexBufferWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_DECL_TASK_HEAD_END

struct HWBuildIndexBufferWriteCommandPush {
    DAXA_TH_BLOB(HWBuildIndexBufferWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using HWBuildIndexBufferWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            HWBuildIndexBufferWriteCommand::Task, 
                                            HWBuildIndexBufferWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(HWBuildIndexBuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_DECL_TASK_HEAD_END

struct HWBuildIndexBufferPush {
    DAXA_TH_BLOB(HWBuildIndexBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using HWBuildIndexBufferTask = foundation::IndirectComputeDispatchTask<
                                            HWBuildIndexBuffer::Task, 
                                            HWBuildIndexBufferPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SWBuildIndexBufferWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_DECL_TASK_HEAD_END

struct SWBuildIndexBufferWriteCommandPush {
    DAXA_TH_BLOB(SWBuildIndexBufferWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SWBuildIndexBufferWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SWBuildIndexBufferWriteCommand::Task, 
                                            SWBuildIndexBufferWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SWBuildIndexBuffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_DECL_TASK_HEAD_END

struct SWBuildIndexBufferPush {
    DAXA_TH_BLOB(SWBuildIndexBuffer, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SWBuildIndexBufferTask = foundation::IndirectComputeDispatchTask<
                                            SWBuildIndexBuffer::Task, 
                                            SWBuildIndexBufferPush, 
                                            "src/graphics/virtual_geometry/tasks/build_index_buffer.slang", 
                                            "build_index_buffer">;
#endif
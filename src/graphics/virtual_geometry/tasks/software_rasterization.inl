#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationWriteCommandPush {
    DAXA_TH_BLOB(SoftwareRasterizationWriteCommand, uses)
    daxa_u32 dummy;
};

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterization)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndexBuffer), u_index_buffer)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, u_visibility_image)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationPush {
    DAXA_TH_BLOB(SoftwareRasterization, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SoftwareRasterizationWriteCommand::Task, 
                                            SoftwareRasterizationWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization.slang", 
                                            "software_rasterization_write_command">;

using SoftwareRasterizationTask = foundation::IndirectComputeDispatchTask<
                                            SoftwareRasterization::Task, 
                                            SoftwareRasterizationPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization.slang", 
                                            "software_rasterization">;
#endif
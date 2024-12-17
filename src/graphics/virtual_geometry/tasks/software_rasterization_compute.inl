#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationComputeWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationComputeWriteCommandPush {
    DAXA_TH_BLOB(SoftwareRasterizationComputeWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationComputeWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SoftwareRasterizationComputeWriteCommand::Task, 
                                            SoftwareRasterizationComputeWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_compute.slang", 
                                            "software_rasterization_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationCompute)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, REGULAR_2D, u_visibility_image)
DAXA_TH_IMAGE_TYPED(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_overdraw_image)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationComputePush {
    DAXA_TH_BLOB(SoftwareRasterizationCompute, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationComputeTask = foundation::IndirectComputeDispatchTask<
                                            SoftwareRasterizationCompute::Task, 
                                            SoftwareRasterizationComputePush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_compute.slang", 
                                            "software_rasterization">;
#endif
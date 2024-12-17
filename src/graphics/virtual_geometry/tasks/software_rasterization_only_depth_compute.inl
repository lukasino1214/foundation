#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepthComputeWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthComputeWriteCommandPush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepthComputeWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationOnlyDepthComputeWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SoftwareRasterizationOnlyDepthComputeWriteCommand::Task, 
                                            SoftwareRasterizationOnlyDepthComputeWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth_compute.slang", 
                                            "software_rasterization_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepthCompute)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletIndices), u_meshlet_indices)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_TYPED(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthComputePush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepthCompute, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationOnlyDepthComputeTask = foundation::IndirectComputeDispatchTask<
                                            SoftwareRasterizationOnlyDepthCompute::Task, 
                                            SoftwareRasterizationOnlyDepthComputePush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth_compute.slang", 
                                            "software_rasterization">;
#endif
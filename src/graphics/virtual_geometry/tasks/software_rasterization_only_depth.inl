#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepthWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthWriteCommandPush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepthWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationOnlyDepthWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            SoftwareRasterizationOnlyDepthWriteCommand::Task, 
                                            SoftwareRasterizationOnlyDepthWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang", 
                                            "software_rasterization_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SoftwareRasterizationOnlyDepth)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsData), u_meshlets_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Material), u_materials)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_TH_IMAGE_TYPED(FRAGMENT_SHADER_STORAGE_READ_WRITE_CONCURRENT, daxa::RWTexture2DId<u32>, u_depth_image)
DAXA_DECL_TASK_HEAD_END

struct SoftwareRasterizationOnlyDepthPush {
    DAXA_TH_BLOB(SoftwareRasterizationOnlyDepth, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SoftwareRasterizationOnlyDepthTask = foundation::IndirectComputeDispatchTask<
                                            SoftwareRasterizationOnlyDepth::Task, 
                                            SoftwareRasterizationOnlyDepthPush, 
                                            "src/graphics/virtual_geometry/tasks/software_rasterization_only_depth.slang", 
                                            "software_rasterization">;
#endif
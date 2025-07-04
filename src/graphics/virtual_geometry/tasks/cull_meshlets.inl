#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsOpaqueWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsOpaqueWriteCommandPush {
    DAXA_TH_BLOB(CullMeshletsOpaqueWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletsOpaqueWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            CullMeshletsOpaqueWriteCommand::Task, 
                                            CullMeshletsOpaqueWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsOpaque)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_SAMPLED, REGULAR_2D, u_hiz)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsOpaquePush {
    DAXA_TH_BLOB(CullMeshletsOpaque, uses)
    daxa_u32 force_hw_rasterization;
};

#if __cplusplus
using CullMeshletsOpaqueTask = foundation::IndirectComputeDispatchTask<
                                            CullMeshletsOpaque::Task, 
                                            CullMeshletsOpaquePush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsMaskedWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsMaskedWriteCommandPush {
    DAXA_TH_BLOB(CullMeshletsMaskedWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletsMaskedWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            CullMeshletsMaskedWriteCommand::Task, 
                                            CullMeshletsMaskedWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsMasked)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_SAMPLED, REGULAR_2D, u_hiz)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsMaskedPush {
    DAXA_TH_BLOB(CullMeshletsMasked, uses)
    daxa_u32 force_hw_rasterization;
};

#if __cplusplus
using CullMeshletsMaskedTask = foundation::IndirectComputeDispatchTask<
                                            CullMeshletsMasked::Task, 
                                            CullMeshletsMaskedPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsTransparentWriteCommand)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsTransparentWriteCommandPush {
    DAXA_TH_BLOB(CullMeshletsTransparentWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CullMeshletsTransparentWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            CullMeshletsTransparentWriteCommand::Task, 
                                            CullMeshletsTransparentWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CullMeshletsTransparent)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(Mesh), u_meshes)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(TransformInfo), u_transforms)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_prefix_sum_work_expansion_mesh)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(MeshesData), u_culled_meshes_data)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_WRITE, daxa_BufferPtr(MeshletsDataMerged), u_meshlets_data_merged)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER_SAMPLED, REGULAR_2D, u_hiz)
DAXA_TH_BUFFER_PTR(COMPUTE_SHADER_READ, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CullMeshletsTransparentPush {
    DAXA_TH_BLOB(CullMeshletsTransparent, uses)
    daxa_u32 force_hw_rasterization;
};

#if __cplusplus
using CullMeshletsTransparentTask = foundation::IndirectComputeDispatchTask<
                                            CullMeshletsTransparent::Task, 
                                            CullMeshletsTransparentPush, 
                                            "src/graphics/virtual_geometry/tasks/cull_meshlets.slang", 
                                            "cull_meshlets">;
#endif
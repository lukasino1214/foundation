#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(AddAnimatedMeshesToPrefixSumWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_animated_mesh_count)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(AnimatedMesh), u_animated_meshes)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct AddAnimatedMeshesToPrefixSumWriteCommandPush {
    DAXA_TH_BLOB(AddAnimatedMeshesToPrefixSumWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using AddAnimatedMeshesToPrefixSumWriteCommandTask = foundation::WriteIndirectComputeDispatchTask<
                                            "AddAnimatedMeshesToPrefixSumWriteCommand",
                                            AddAnimatedMeshesToPrefixSumWriteCommand::Task, 
                                            AddAnimatedMeshesToPrefixSumWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "add_animated_meshes_to_prefix_sum_write_command">;
#endif


DAXA_DECL_TASK_HEAD_BEGIN(AddAnimatedMeshesToPrefixSum)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(u32), u_animated_mesh_count)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(AnimatedMesh), u_animated_meshes)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_vertices_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_meshlets_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct AddAnimatedMeshesToPrefixSumPush {
    DAXA_TH_BLOB(AddAnimatedMeshesToPrefixSum, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using AddAnimatedMeshesToPrefixSumTask = foundation::IndirectComputeDispatchTask<
                                            "AddAnimatedMeshesToPrefixSum",
                                            AddAnimatedMeshesToPrefixSum::Task, 
                                            AddAnimatedMeshesToPrefixSumPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "add_animated_meshes_to_prefix_sum">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SkinAnimatedMeshesWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_vertices_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SkinAnimatedMeshesWriteCommandPush {
    DAXA_TH_BLOB(SkinAnimatedMeshesWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SkinAnimatedMeshesWriteCommandTask = foundation::IndirectComputeDispatchTask<
                                            "SkinAnimatedMeshesWriteCommand",
                                            SkinAnimatedMeshesWriteCommand::Task, 
                                            SkinAnimatedMeshesWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "skin_animated_meshes_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(SkinAnimatedMeshes)
DAXA_TH_BUFFER_PTR(READ_WRITE, daxa_BufferPtr(AnimatedMesh), u_animated_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_vertices_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct SkinAnimatedMeshesPush {
    DAXA_TH_BLOB(SkinAnimatedMeshes, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using SkinAnimatedMeshesTask = foundation::IndirectComputeDispatchTask<
                                            "SkinAnimatedMeshes",
                                            SkinAnimatedMeshes::Task, 
                                            SkinAnimatedMeshesPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "skin_animated_meshes">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CalculateBoundsAnimatedMeshesWriteCommand)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_meshlets_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CalculateBoundsAnimatedMeshesWriteCommandPush {
    DAXA_TH_BLOB(CalculateBoundsAnimatedMeshesWriteCommand, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CalculateBoundsAnimatedMeshesWriteCommandTask = foundation::IndirectComputeDispatchTask<
                                            "CalculateBoundsAnimatedMeshesWriteCommand",
                                            CalculateBoundsAnimatedMeshesWriteCommand::Task, 
                                            CalculateBoundsAnimatedMeshesWriteCommandPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "calculate_bounds_animated_meshes_write_command">;
#endif

DAXA_DECL_TASK_HEAD_BEGIN(CalculateBoundsAnimatedMeshes)
DAXA_TH_BUFFER_PTR(READ_WRITE, daxa_BufferPtr(AnimatedMesh), u_animated_meshes)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(PrefixSumWorkExpansion), u_animated_mesh_meshlets_prefix_sum_work_expansion)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(DispatchIndirectStruct), u_command)
DAXA_DECL_TASK_HEAD_END

struct CalculateBoundsAnimatedMeshesPush {
    DAXA_TH_BLOB(CalculateBoundsAnimatedMeshes, uses)
    daxa_u32 dummy;
};

#if __cplusplus
using CalculateBoundsAnimatedMeshesTask = foundation::IndirectComputeDispatchTask<
                                            "CalculateBoundsAnimatedMeshes",
                                            CalculateBoundsAnimatedMeshes::Task, 
                                            CalculateBoundsAnimatedMeshesPush, 
                                            "src/graphics/virtual_geometry/tasks/skin_animated_meshes.slang", 
                                            "calculate_bounds_animated_meshes">;
#endif
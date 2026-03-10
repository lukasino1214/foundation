#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"


DAXA_DECL_TASK_HEAD_BEGIN(GTAO)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<u32>, u_normal_image)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<f32>, u_depth_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<f32>, u_gtao_image)
DAXA_DECL_TASK_HEAD_END

struct GTAOPush {
    DAXA_TH_BLOB(GTAO, uses)
    u32 dummy;
};

#if __cplusplus
using GTAOTask = foundation::ComputeDispatchTask<
    "GTAO", 
    GTAO::Task, 
    GTAOPush, 
    "src/graphics/tasks/gtao.slang", 
    "gtao"
>;
#endif

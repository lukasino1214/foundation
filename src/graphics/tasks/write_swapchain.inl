#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../shader_library/shared.inl"

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(WriteSwapchain)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32vec4>, u_render_image)
DAXA_TH_IMAGE_TYPED(WRITE, daxa::RWTexture2DId<daxa_f32vec4>, u_swapchain_image)
DAXA_DECL_TASK_HEAD_END

struct WriteSwapchainPush {
    DAXA_TH_BLOB(WriteSwapchain, uses)
    u32vec2 size;
};

#if __cplusplus
using WriteSwapchainTask = foundation::ComputeDispatchTask<
    "WriteSwapchain", 
    WriteSwapchain::Task, 
    WriteSwapchainPush, 
    "src/graphics/tasks/write_swapchain.slang", 
    "write_swapchain"
>;
#endif
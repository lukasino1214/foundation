#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

#define GEN_HIZ_X 16
#define GEN_HIZ_Y 16
#define GEN_HIZ_LEVELS_PER_DISPATCH 12
#define GEN_HIZ_WINDOW_X 64
#define GEN_HIZ_WINDOW_Y 64

DAXA_DECL_TASK_HEAD_BEGIN(GenerateHiz)
DAXA_TH_BUFFER_PTR(READ_WRITE_CONCURRENT, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_IMAGE_ID(SAMPLED, REGULAR_2D, u_src)
DAXA_TH_IMAGE_ID_MIP_ARRAY(READ_WRITE, REGULAR_2D, u_mips, GEN_HIZ_LEVELS_PER_DISPATCH)
DAXA_DECL_TASK_HEAD_END

struct GenerateHizPush {
    DAXA_TH_BLOB(GenerateHiz, uses)
    daxa_RWBufferPtr(daxa_u32) counter;
    daxa_u32 mip_count;
    daxa_u32 total_workgroup_count;
};

#if __cplusplus
struct GenerateHizTask : GenerateHiz::Task {
    AttachmentViews views = {};
    foundation::Context* context = {};

    static auto name() -> std::string_view {
        return "GenerateHiz";
    }

    static auto pipeline_config_info() -> daxa::ComputePipelineCompileInfo2 {
        return {
            .source = daxa::ShaderFile{"src/graphics/virtual_geometry/tasks/generate_hiz.glsl"},
            .push_constant_size = s_cast<u32>(sizeof(GenerateHizPush)),
            .name = std::string{name()},
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[name()]->start(ti.recorder);
        ti.recorder.set_pipeline(*context->compute_pipelines.at(name()));
        daxa_u32vec2 next_higher_po2_render_target_size = {
            context->shader_globals.next_lower_po2_render_target_size.x,
            context->shader_globals.next_lower_po2_render_target_size.y,
        };
        auto const dispatch_x = round_up_div(next_higher_po2_render_target_size.x * 2, GEN_HIZ_WINDOW_X);
        auto const dispatch_y = round_up_div(next_higher_po2_render_target_size.y * 2, GEN_HIZ_WINDOW_Y);
        GenerateHizPush push = {
            .uses = {},
            .counter = ti.allocator->allocate_fill(0u).value().device_address,
            .mip_count = ti.get(AT.u_mips).view.slice.level_count,
            .total_workgroup_count = dispatch_x * dispatch_y,
        };
        std::memcpy(push.uses.value.data(), ti.attachment_shader_blob.data(), ti.attachment_shader_blob.size());
        ti.recorder.push_constant(push);
        ti.recorder.dispatch({.x = dispatch_x, .y = dispatch_y, .z = 1});
        context->gpu_metrics[name()]->end(ti.recorder);
    }
};
#endif
#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../../shader_library/shared.inl"

#define GEN_HIZ_X 16
#define GEN_HIZ_Y 16
#define GEN_HIZ_LEVELS_PER_DISPATCH 16
#define GEN_HIZ_WINDOW_X 64
#define GEN_HIZ_WINDOW_Y 64

#if DAXA_LANGUAGE != DAXA_LANGUAGE_GLSL

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(GenHiz)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), globals)
DAXA_TH_IMAGE_TYPED(SAMPLED, daxa::Texture2DId<f32>, src)
DAXA_TH_IMAGE_TYPED_MIP_ARRAY(READ_WRITE, daxa::RWTexture2DIndex<f32>, hiz, GEN_HIZ_LEVELS_PER_DISPATCH)
DAXA_DECL_TASK_HEAD_END

struct GenHizPush {
    GenHiz::AttachmentShaderBlob attach;
    daxa_RWBufferPtr(daxa_u32) workgroup_finish_counter;
    u32 total_workgroup_count;
    u32 mip_count;
    u32vec2 dst_mip0_size;
    u32vec2 src_size;
};

#endif

#if defined(__cplusplus)

#include <graphics/gpu_context.hpp>

namespace foundation {
    inline auto gen_hiz_pipeline_config_info() -> daxa::ComputePipelineCompileInfo2 {
        const std::string_view shader_path_sv = "src/graphics/tasks/gen_hiz.slang";
        const std::string_view entry_point_sv = "entry_gen_hiz";
        
        return daxa::ComputePipelineCompileInfo2 {
            .source = daxa::ShaderFile { std::filesystem::path(shader_path_sv) },
            .entry_point = std::string(entry_point_sv),
            .language = daxa::ShaderLanguage::SLANG,
            .defines = {{ std::string("gen_hiz") + "_SHADER", "1"} },
            .push_constant_size = s_cast<u32>(sizeof(GenHizPush)),
            .name = std::string("gen_hiz"),
        };
    }

    inline auto fill_cull_data(GPUContext const & render_context) -> CullData {
        CullData ret = {};
        const auto rt_size = render_context.shader_globals.render_target_size;

        ret.hiz_size = { (rt_size.x + 1) / 2, (rt_size.y + 1) / 2 };
        ret.hiz_size_rcp = { 1.0f / static_cast<f32>(ret.hiz_size.x), 1.0f / static_cast<f32>(ret.hiz_size.y) };

        ret.physical_hiz_size = { std::bit_ceil( ret.hiz_size.x ), std::bit_ceil( ret.hiz_size.y ) };
        ret.physical_hiz_size_rcp = { 1.0f / static_cast<f32>(ret.physical_hiz_size.x), 1.0f / static_cast<f32>(ret.physical_hiz_size.y) };

        return ret;
    }

    struct TaskGenHizSinglePassInfo {
        GPUContext* gpu_context = {};
        daxa::TaskGraph& tg;
        daxa::TaskImageView src = {};
        daxa::TaskBufferView globals = {};
        daxa::TaskImageView* hiz = {};
    };

    inline void task_gen_hiz_single_pass(const TaskGenHizSinglePassInfo& info) {
        const u32vec2 hiz_size = {
            std::max(1u, info.gpu_context->shader_globals.cull_data.physical_hiz_size.x),
            std::max(1u, info.gpu_context->shader_globals.cull_data.physical_hiz_size.y),
        };
        const u32 mip_count = 1 + static_cast<u32>(std::floor(std::log2(std::max(hiz_size.x, hiz_size.y))));
        *info.hiz = info.tg.create_transient_image({
            .format = daxa::Format::R32_SFLOAT,
            .size = {hiz_size.x, hiz_size.y, 1},
            .mip_level_count = mip_count,
            .name = std::string("hiz "),
        });

        daxa::Task hiz_task = daxa::Task("GenHiz")
            .uses_head<GenHiz::Info>()
            .head_views({
                .globals = info.globals,
                .src = info.src,
                .hiz = *info.hiz,
            })
            .executes([=](daxa::TaskInterface ti)
            {
                const auto& AT = GenHiz::Info::AT;
                ti.recorder.set_pipeline(*info.gpu_context->compute_pipelines.at(gen_hiz_pipeline_config_info().name));
                const auto dispatch_x = round_up_div(hiz_size.x * 2, GEN_HIZ_WINDOW_X);
                const auto dispatch_y = round_up_div(hiz_size.y * 2, GEN_HIZ_WINDOW_Y);
                GenHizPush push = {
                    .attach = ti.attachment_shader_blob,
                    .workgroup_finish_counter = ti.allocator->allocate_fill(0u).value().device_address,
                    .total_workgroup_count = dispatch_x * dispatch_y,
                    .mip_count = mip_count,
                    .dst_mip0_size = hiz_size,
                    .src_size = u32vec2{ ti.info(AT.src).value().size.x, ti.info(AT.src).value().size.y },
                };
                ti.recorder.push_constant(push);
                
                ti.recorder.dispatch({ dispatch_x, dispatch_y, 1 });
            });
        info.tg.add_task(hiz_task);
    }
}

#endif

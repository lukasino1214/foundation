#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>
#include "../../shader_library/shared.inl"

#define LUM_HISTOGRAM_BIN_COUNT 256
#define SUM_HISTOGRAM_INDEX 256
#define COMPUTE_HISTOGRAM_WG_X 32
#define COMPUTE_HISTOGRAM_WG_Y 32

struct AutoExposureState {
    f32 exposure;
    f32 ev;
};

DAXA_DECL_BUFFER_PTR(AutoExposureState)

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(GenLuminanceHistogramH)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), globals)
DAXA_TH_BUFFER_PTR(WRITE, daxa_BufferPtr(daxa_u32), histogram)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(AutoExposureState), exposure_state)
DAXA_TH_IMAGE_TYPED(READ, daxa::RWTexture2DId<f32vec4>, color_image)
DAXA_DECL_TASK_HEAD_END

DAXA_DECL_COMPUTE_TASK_HEAD_BEGIN(GenLuminanceAverageH)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), globals)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(daxa_u32), histogram)
DAXA_TH_BUFFER_PTR(READ_WRITE, daxa_BufferPtr(AutoExposureState), exposure_state)
DAXA_DECL_TASK_HEAD_END

#if __cplusplus
#include "../gpu_context.hpp"

inline auto gen_luminace_histogram_pipeline_compile_info() -> daxa::ComputePipelineCompileInfo2 {
    return {
        .source = daxa::ShaderFile{"./src/graphics/tasks/auto_exposure.hlsl"},
        .entry_point = "gen_histogram",
        .defines = {{"GEN_HISTOGRAM", "1"}},
        .push_constant_size = sizeof(GenLuminanceHistogramH::AttachmentShaderBlob),
        .name = std::string{GenLuminanceHistogramH::Info::NAME},
    };
};

inline auto gen_luminace_average_pipeline_compile_info() -> daxa::ComputePipelineCompileInfo2 {
    return {
        .source = daxa::ShaderFile{"./src/graphics/tasks/auto_exposure.hlsl"},
        .entry_point = "gen_average",
        .defines = {{"GEN_AVERAGE", "1"}},
        .push_constant_size = sizeof(GenLuminanceAverageH::AttachmentShaderBlob),
        .name = std::string{GenLuminanceAverageH::Info::NAME},
    };
};

inline void gen_luminance_histogram_callback(daxa::TaskInterface ti, foundation::GPUContext* gpu_context) {
    const auto offscreen_resolution = gpu_context->shader_globals.render_target_size;
    const auto dispatch_size = u32vec2 {
        (offscreen_resolution.x + COMPUTE_HISTOGRAM_WG_X - 1) / COMPUTE_HISTOGRAM_WG_X,
        (offscreen_resolution.y + COMPUTE_HISTOGRAM_WG_Y - 1) / COMPUTE_HISTOGRAM_WG_Y,
    };

    ti.recorder.set_pipeline(*gpu_context->compute_pipelines.at(gen_luminace_histogram_pipeline_compile_info().name));

    GenLuminanceHistogramH::AttachmentShaderBlob push = ti.attachment_shader_blob;
    ti.recorder.push_constant(push);
    ti.recorder.dispatch({ .x = dispatch_size.x, .y = dispatch_size.y });
}

inline void gen_luminance_average_callback(daxa::TaskInterface ti, foundation::GPUContext* gpu_context) {
    ti.recorder.set_pipeline(*gpu_context->compute_pipelines.at(gen_luminace_average_pipeline_compile_info().name));

    GenLuminanceAverageH::AttachmentShaderBlob push = ti.attachment_shader_blob;
    ti.recorder.push_constant(push);
    ti.recorder.dispatch({ .x = 1 });
}
#endif

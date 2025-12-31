#pragma once
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#if defined(__cplusplus)
#include <graphics/misc.hpp>
#endif

#include "../../../shader_library/shared.inl"

DAXA_DECL_TASK_HEAD_BEGIN(RayTrace)
DAXA_TH_BUFFER_PTR(READ, daxa_BufferPtr(ShaderGlobals), u_globals)
DAXA_TH_TLAS_ID(READ, tlas)
DAXA_TH_IMAGE_ID(WRITE, REGULAR_2D, u_image)
DAXA_DECL_TASK_HEAD_END

struct RayTracePush {
    DAXA_TH_BLOB(RayTrace, uses)
    daxa_u32 dummy;
};

#if __cplusplus
#include "graphics/context.hpp"

struct RayTraceTask : RayTrace::Task {
    RayTraceTask::Task::AttachmentViews views = {};
    foundation::Context* context = {};
    RayTracePush push = {};

    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    static auto name() -> std::string_view {
        return "RayTrace";
    }

    static auto pipeline_config_info() -> daxa::RayTracingPipelineCompileInfo2 {
        return {
            .ray_gen_infos = {daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderFile{"src/graphics/virtual_geometry/tasks/ray_tracing.slang"},
                .entry_point = "ray_gen",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{RayTraceTask::name()} + "_SHADER", "1" } },
            }},
            .any_hit_infos = {daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderFile{"src/graphics/virtual_geometry/tasks/ray_tracing.slang"},
                .entry_point = "any_hit",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{RayTraceTask::name()} + "_SHADER", "1" } },
            }},
            .closest_hit_infos = {daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderFile{"src/graphics/virtual_geometry/tasks/ray_tracing.slang"},
                .entry_point = "closest_hit",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{RayTraceTask::name()} + "_SHADER", "1" } },
            }},
            .miss_hit_infos = {daxa::ShaderCompileInfo2 {
                .source = daxa::ShaderFile{"src/graphics/virtual_geometry/tasks/ray_tracing.slang"},
                .entry_point = "miss",
                .language = daxa::ShaderLanguage::SLANG,
                .defines = { { std::string{RayTraceTask::name()} + "_SHADER", "1" } },
            }},
            .shader_groups_infos = {
                // Gen Group
                daxa::RayTracingShaderGroupInfo{
                    .type = daxa::ShaderGroup::GENERAL,
                    .general_shader_index = 0,
                },
                // Miss group
                daxa::RayTracingShaderGroupInfo{
                    .type = daxa::ShaderGroup::GENERAL,
                    .general_shader_index = 3,
                },
                // Hit group
                daxa::RayTracingShaderGroupInfo{
                    .type = daxa::ShaderGroup::TRIANGLES_HIT_GROUP,
                    .closest_hit_shader_index = 2,
                },
                daxa::RayTracingShaderGroupInfo{
                    .type = daxa::ShaderGroup::TRIANGLES_HIT_GROUP,
                    .closest_hit_shader_index = 2,
                    .any_hit_shader_index = 1,
                },
            },
            .max_ray_recursion_depth = 1,
            .push_constant_size = sizeof(RayTracePush),
            .name = std::string{RayTraceTask::name()}
        };
    }

    void callback(daxa::TaskInterface ti) {
        context->gpu_metrics[this->name()]->start(ti.recorder);
        u32 size_x = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.x;
        u32 size_y = ti.device.image_info(ti.get(AT.u_image).ids[0]).value().size.y;

        ti.recorder.set_pipeline(*context->ray_tracing_pipelines.at(this->name())->pipeline);

        assign_blob(push.uses, ti.attachment_shader_blob);
        ti.recorder.push_constant(push);

        ti.recorder.trace_rays({
            .width = size_x,
            .height = size_y,
            .depth = 1,
            .shader_binding_table = context->ray_tracing_pipelines.at(this->name())->sbt,
        });
        context->gpu_metrics[this->name()]->end(ti.recorder);
    }
};
#endif
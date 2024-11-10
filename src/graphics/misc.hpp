#pragma once

#include <pch.hpp>
#include <graphics/context.hpp>

namespace foundation {
    void assign_blob(auto & arr, auto const & span) {
        std::memcpy(arr.value.data(), span.data(), span.size());
    }

    template <typename T_USES_BASE, typename T_PUSH, daxa::StringLiteral T_FILE_PATH, daxa::StringLiteral T_ENTRY_POINT>
    struct WriteIndirectComputeDispatchTask : T_USES_BASE {
        T_USES_BASE::AttachmentViews views = {};
        Context* context = {};
        T_PUSH push = {};

        static auto pipeline_config_info() -> daxa::ComputePipelineCompileInfo {
            auto const shader_path_sv = std::string_view(T_FILE_PATH.value, T_FILE_PATH.SIZE);
            auto const entry_point_sv = std::string_view(T_ENTRY_POINT.value, T_ENTRY_POINT.SIZE);
            daxa::ShaderLanguage lang = shader_path_sv.ends_with(".glsl") ? daxa::ShaderLanguage::GLSL : daxa::ShaderLanguage::SLANG;
            return daxa::ComputePipelineCompileInfo {
                .shader_info = daxa::ShaderCompileInfo {
                    .source = daxa::ShaderFile { std::filesystem::path(shader_path_sv) },
                    .compile_options = {
                        .entry_point = std::string(entry_point_sv),
                        .language = lang,
                        .defines = {{ std::string(T_USES_BASE::name()) + "_SHADER", "1"} },
                    },
                },
                .push_constant_size = s_cast<u32>(sizeof(T_PUSH)),
                .name = std::string(T_USES_BASE::name()),
            };
        }

        void callback(daxa::TaskInterface ti) {
            context->gpu_metrics[this->name()]->start(ti.recorder);
            ti.recorder.set_pipeline(*context->compute_pipelines.at(T_USES_BASE::name()));
            assign_blob(push.uses, ti.attachment_shader_blob);
            ti.recorder.push_constant(push);
            ti.recorder.dispatch({.x = 1, .y = 1, .z = 1});
            context->gpu_metrics[this->name()]->end(ti.recorder);
        }
    };

    template <typename T_USES_BASE, typename T_PUSH, daxa::StringLiteral T_FILE_PATH, daxa::StringLiteral T_ENTRY_POINT>
    struct IndirectComputeDispatchTask : T_USES_BASE {
        T_USES_BASE::AttachmentViews views = {};
        Context* context = {};
        T_PUSH push = {};

        static auto pipeline_config_info() -> daxa::ComputePipelineCompileInfo {
            auto const shader_path_sv = std::string_view(T_FILE_PATH.value, T_FILE_PATH.SIZE);
            auto const entry_point_sv = std::string_view(T_ENTRY_POINT.value, T_ENTRY_POINT.SIZE);
            daxa::ShaderLanguage lang = shader_path_sv.ends_with(".glsl") ? daxa::ShaderLanguage::GLSL : daxa::ShaderLanguage::SLANG;
            return daxa::ComputePipelineCompileInfo {
                .shader_info = daxa::ShaderCompileInfo {
                    .source = daxa::ShaderFile { std::filesystem::path(shader_path_sv) },
                    .compile_options = {
                        .entry_point = std::string(entry_point_sv),
                        .language = lang,
                        .defines = {{ std::string(T_USES_BASE::name()) + "_SHADER", "1"} },
                    },
                },
                .push_constant_size = s_cast<u32>(sizeof(T_PUSH)),
                .name = std::string(T_USES_BASE::name()),
            };
        }

        void callback(daxa::TaskInterface ti) {
            context->gpu_metrics[this->name()]->start(ti.recorder);
            ti.recorder.set_pipeline(*context->compute_pipelines.at(T_USES_BASE::name()));
            assign_blob(push.uses, ti.attachment_shader_blob);
            ti.recorder.push_constant(push);
            ti.recorder.dispatch_indirect({
                .indirect_buffer = ti.get(this->AT.u_command).ids[0],
            });
            context->gpu_metrics[this->name()]->end(ti.recorder);
        }
    };

    template <typename T_USES_BASE, typename T_PUSH, const char* T_FILE_PATH>
    struct ComputeDispatchTask : T_USES_BASE {
        T_USES_BASE::AttachmentViews views = {};
        Context* context = {};
        T_PUSH push = {};
        std::function<daxa::DispatchInfo(void)> dispatch_callback = {};

        static auto pipeline_config_info() -> daxa::ComputePipelineCompileInfo {
            auto const shader_path_sv = std::string_view(T_FILE_PATH);
            daxa::ShaderLanguage lang = shader_path_sv.ends_with(".glsl") ? daxa::ShaderLanguage::GLSL : daxa::ShaderLanguage::SLANG;
            return daxa::ComputePipelineCompileInfo {
                .shader_info = daxa::ShaderCompileInfo {
                    .source = daxa::ShaderFile { std::filesystem::path(shader_path_sv) },
                    .compile_options = {
                        .entry_point = std::string("main"),
                        .language = lang,
                        .defines = {{ std::string(T_USES_BASE::name()) + "_SHADER", "1"} },
                    },
                },
                .push_constant_size = s_cast<u32>(sizeof(T_PUSH)),
                .name = std::string(T_USES_BASE::name()),
            };
        }

        void callback(daxa::TaskInterface ti) {
            context->gpu_metrics[this->name()]->start(ti.recorder);
            ti.recorder.set_pipeline(*context->compute_pipelines.at(T_USES_BASE::name()));
            assign_blob(push.uses, ti.attachment_shader_blob);
            ti.recorder.push_constant(push);
            ti.recorder.dispatch(dispatch_callback());
            context->gpu_metrics[this->name()]->end(ti.recorder);
        }
    };
}
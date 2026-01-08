#include "gpu_context.hpp"
#include <graphics/window.hpp>

#include "helper.hpp"

namespace foundation {
    GPUContext::GPUContext(NativeWindow* window)
        : instance{daxa::create_instance({})},
            device{[&](){
                daxa::ImplicitFeatureFlags required_implicit = 
                    daxa::ImplicitFeatureFlagBits::MESH_SHADER |
                    daxa::ImplicitFeatureFlagBits::BASIC_RAY_TRACING |
                    daxa::ImplicitFeatureFlagBits::RAY_TRACING_PIPELINE |
                    daxa::ImplicitFeatureFlagBits::RAY_TRACING_INVOCATION_REORDER |
                    daxa::ImplicitFeatureFlagBits::SHADER_ATOMIC_INT64 |
                    daxa::ImplicitFeatureFlagBits::IMAGE_ATOMIC64 |
                    daxa::ImplicitFeatureFlagBits::SWAPCHAIN;

                daxa::DeviceInfo2 device_info = {
                    .explicit_features = daxa::ExplicitFeatureFlagBits::BUFFER_DEVICE_ADDRESS_CAPTURE_REPLAY,
                    .max_allowed_images = 100000,
                    .max_allowed_buffers = 100000,
                };

                device_info = this->instance.choose_device(required_implicit, device_info);
                return this->instance.create_device_2(device_info); 
            }()},
            swapchain{window->create_swapchain(this->device)},
            pipeline_manager{daxa::PipelineManagerInfo2{
                .device = device,
                .root_paths = {
                        DAXA_SHADER_INCLUDE_DIR,
                        "./",
                        ""
                    },
                .write_out_preprocessed_code = "assets/cache/preproc",
                .write_out_spirv = "assets/cache/spv_raw",
                .spirv_cache_folder = "assets/cache/spv",
                .default_enable_debug_info = true,
                .name = "pipeline_manager",
            }},
            shader_globals{}, 
            shader_globals_buffer{make_task_buffer(this, {
                            .size = s_cast<u32>(sizeof(ShaderGlobals)),
                            .name = "globals",
                        })} {
    }

    GPUContext::~GPUContext() {
        device.destroy_buffer(shader_globals_buffer.get_state().buffers[0]);
        device.wait_idle();
        device.collect_garbage();
    }
}

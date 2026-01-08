#include <application.hpp>
#include <imgui.h>

namespace foundation {
    Application::Application() : 
        window{1280, 720, "Foundation"},
        context{this->window},
        thread_pool{std::make_unique<ThreadPool>(std::thread::hardware_concurrency() - 1)},
        renderer{ std::make_unique<Renderer>(&window, &context)} {
        PROFILE_SCOPE;

        struct CompilePipelinesTask : Task {
            Renderer* renderer = {};
            explicit CompilePipelinesTask(Renderer* renderer) : renderer{renderer} { chunk_count = 1; }

            void callback(u32 /*chunk_index*/, u32 /*thread_index*/) override {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                renderer->compile_pipelines();
                renderer->pipelines_compiled = true;
                renderer->rebuild_task_graph();
            }
        };

        auto compile_pipelines_task = std::make_shared<CompilePipelinesTask>(renderer.get());
        thread_pool->async_dispatch(compile_pipelines_task);
    
        last_time_point = std::chrono::steady_clock::now();
        main_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));
        observer_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));
    }

    Application::~Application() = default;

    auto Application::run() -> i32 {
        while(!window.window_state->close_requested) {
            PROFILE_FRAME_START("App Run");
            PROFILE_SCOPE;
            auto new_time_point = std::chrono::steady_clock::now();
            this->delta_time = std::chrono::duration_cast<std::chrono::duration<f32, std::chrono::milliseconds::period>>(new_time_point - this->last_time_point).count() * 0.001f;
            this->last_time_point = new_time_point;
            window.update();

            if(window.window_state->resize_requested) {
                renderer->window_resized();
                window.window_state->resize_requested = false;
            }

            update();
            renderer->render();
            PROFILE_FRAME_END("App Run");
        }

        return 0;
    }

    void Application::update() {
        PROFILE_SCOPE;
        {
            PROFILE_ZONE_NAMED(ui_render_start);
            renderer->ui_render_start();
        }
        {
            PROFILE_ZONE_NAMED(ui_update_);
            ui_update();
        }
        {
            PROFILE_ZONE_NAMED(ui_render_end);
            renderer->ui_render_end();
        }
        
        {
            PROFILE_ZONE_NAMED(record_manifest_update);
            auto size = context.device.image_info(renderer->render_image.get_state().images[0]).value().size;
            context.update_shader_globals(main_camera, observer_camera, {size.x, size.y});
        }

        {
            PROFILE_ZONE_NAMED(scene_update);
        }
    }

    void Application::ui_update() {
        renderer->ui_update();
    }
}
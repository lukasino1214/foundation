#include "viewport_panel.hpp"

#include <utility>

namespace foundation {
    ViewportPanel::ViewportPanel(Context* _context, AppWindow* _window, daxa::ImGuiRenderer* _imgui_renderer, ResizeCallback  _resize_callback)
        : context{_context}, window{_window}, imgui_renderer{_imgui_renderer}, resize_callback{std::move(_resize_callback)} {}

    void ViewportPanel::draw(ControlledCamera3D& camera, f32 delta_time, daxa::TaskImage& color_image) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Viewport");
        ImVec2 size = ImGui::GetContentRegionAvail();
        if(viewport_size != *r_cast<glm::vec2*>(&size)) {
            viewport_size = *r_cast<glm::vec2*>(&size);
            camera.camera.resize(static_cast<i32>(size.x), static_cast<i32>(size.y));
            resize_callback({ size.x, size.y });
        }

        if(ImGui::IsWindowHovered()) { if(window->button_just_pressed(GLFW_MOUSE_BUTTON_1)) { window->capture_cursor(); } }
        if(window->button_just_released(GLFW_MOUSE_BUTTON_1)) { window->release_cursor(); }
        camera.update(*window, delta_time);

        ImTextureID image = imgui_renderer->create_texture_id(daxa::ImGuiImageContext {
            .image_view_id = color_image.get_state().images[0].default_view(),
            .sampler_id = std::bit_cast<daxa::SamplerId>(context->shader_globals.samplers.nearest_clamp)
        });

        ImGui::Image(image, size);

        f32 length = 24.0f;

        ImVec2 pos = ImGui::GetWindowContentRegionMin();

        pos.x += ImGui::GetWindowPos().x;
        pos.y += ImGui::GetWindowPos().y;

        glm::vec2 origin = *r_cast<glm::vec2*>(&pos) + glm::vec2{size.x - length - 8.0f,  length + 8.0f};

        auto project_axis = [&](const glm::vec3& axis) -> ImVec2 {
            return ImVec2(
                origin.x + axis.x * length,
                origin.y + axis.y * length
            );
        };

        auto view_rotation = glm::mat3(camera.camera.view_mat);
        ImVec2 end_x = project_axis(view_rotation * glm::vec3{1.0f, 0.0f, 0.0f});
        ImVec2 end_y = project_axis(view_rotation * glm::vec3{0.0f, -1.0f, 0.0f});
        ImVec2 end_z = project_axis(view_rotation * glm::vec3{0.0f, 0.0f, 1.0f});

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        ImU32 color_x = IM_COL32(255, 0, 0, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_x, color_x, 2.0f);
        draw_list->AddText(end_x, color_x, "X");

        ImU32 color_y = IM_COL32(0, 255, 0, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_y, color_y, 2.0f);
        draw_list->AddText(end_y, color_y, "Y");

        ImU32 color_z = IM_COL32(0, 0, 255, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_z, color_z, 2.0f);
        draw_list->AddText(end_z, color_z, "Z");

        ImGui::End();

        ImGui::PopStyleVar();
    }
}
#include "file_browser.hpp"

#include <ui/ui.hpp>

namespace foundation {
    FileBrowser::FileBrowser() {
        current_path = std::filesystem::current_path();
    }

    void FileBrowser::draw() {
        ImGui::Begin("File Browser");
        ImGui::Text("%ls", current_path.c_str());
        ImGui::End();
    }
}
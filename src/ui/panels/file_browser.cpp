#include "file_browser.hpp"

#include <ui/ui.hpp>

namespace foundation {
    static constexpr u32 thumbnail_size = 96;
    static constexpr u32 padding = 12;

    FileBrowser::FileBrowser() {
        root_path = std::filesystem::current_path();
        update(root_path);
    }

    void FileBrowser::update(const std::filesystem::path& path) {
        bool switch_current_path = path != current_path;

        std::filesystem::file_time_type last_write = std::filesystem::last_write_time(path);
        bool path_updated = last_write != cached_time_stamp;

        if(switch_current_path || path_updated) {
            cached_files.clear();
            cached_folders.clear();

            for(const auto& entry : std::filesystem::directory_iterator(path)) {
                if(entry.is_directory()) {
                    cached_folders.push_back(entry.path().filename());
                } else {
                    cached_files.push_back(entry.path().filename());
                }
            }

            std::vector<TreeNode> new_tree_hiearchy = {};
            new_tree_hiearchy.push_back({ .path = root_path });

            if(tree_hiearchy.empty()) {
                tree_hiearchy.push_back({ .path = root_path });
            }

            auto visit = [&](this auto& self, const std::filesystem::path& search_path, const TreeNode& previous_parent_node, TreeNode& parent_node) -> void {
                for(const auto& entry : std::filesystem::directory_iterator(search_path)) {
                    if(entry.is_directory()) {
                        TreeNode previous_child_node;
                        for(const auto& search_node : previous_parent_node.children) {
                            if(entry.path() == search_node.path) { previous_child_node = search_node; break; }
                        }

                        TreeNode child_node;
                        child_node.is_open = previous_child_node.is_open;
                        child_node.path = entry.path();
                        self(entry.path(), previous_child_node, child_node);
                        parent_node.children.push_back(child_node);
                    }
                }
            };

            visit(root_path, tree_hiearchy[0], new_tree_hiearchy[0]);
            tree_hiearchy = new_tree_hiearchy;
        }

        current_path = path;
        cached_time_stamp = last_write;
    }

    void FileBrowser::draw() {
        update(current_path);

        ImGui::Begin("File Browser");
        ImGui::Text("%ls", current_path.c_str());
        ImGui::SameLine();
        if(ImGui::Button("go back")) {
            update(current_path.parent_path());
        }
        ImGui::SameLine();
        ImGui::Text("%ls", selected_path.c_str());

        ImGui::BeginChild("File Hiearchy", ImVec2(0,0), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeX);
        
        bool folder_clicked = false;
        std::filesystem::path clicked_path = {};

        auto visit = [&](this auto& self, TreeNode& node) -> void {
            ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_None;
            if(node.children.empty()) { tree_node_flags |= ImGuiTreeNodeFlags_Leaf; }
            if(node.path == current_path) { tree_node_flags |= ImGuiTreeNodeFlags_Selected; }

            ImGui::SetNextItemOpen(node.is_open || current_path.string().find(node.path.string()) != std::string::npos);
            node.is_open = ImGui::TreeNodeEx(node.path.filename().string().c_str(), tree_node_flags) ;
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
                folder_clicked = true;
                clicked_path = node.path;
            }

            ImGui::SameLine();
            ui::indent(8, 1);

            if(node.is_open) {
                std::vector<u32> with_children = {};
                std::vector<u32> without_children = {};

                for(u32 i = 0; i < node.children.size(); i++) {
                    const auto& child = node.children[i];
                    if(child.children.empty()) { without_children.push_back(i); }
                    else { with_children.push_back(i); }
                }

                for(u32 index : with_children) { self(node.children[index]); }
                for(u32 index : without_children) { self(node.children[index]); }

                ImGui::TreePop();
            }
        };


        visit(tree_hiearchy[0]);

        ImGui::EndChild();
        
        ImGui::SameLine();

        if(folder_clicked) {
            update(clicked_path);
            folder_clicked = false;
        }

        ImGui::BeginChild("Folder Content", ImVec2(0,0), ImGuiChildFlags_Border);

        f32 width = ImGui::GetContentRegionAvail().x;
        i32 cell_size = thumbnail_size + padding;
        i32 column_count = std::max(s_cast<i32>(width) / cell_size, 1);

        ImGui::Columns(column_count, nullptr, false);


        auto draw_thumbnail = [&](const auto& vec, bool is_folder){
            for(const auto& item : vec) {
                ImGui::PushID(item.string().c_str());
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() / 2) - (s_cast<f32>(thumbnail_size) / 2));

                if(current_path / item == selected_path) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]); }
                ImGui::Button(std::string{std::string(is_folder ? "folder" : "file") + "###"}.c_str(), ImVec2{thumbnail_size, thumbnail_size});
                if(current_path / item == selected_path) { ImGui::PopStyleColor(); }

                if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    selected_path = current_path / item;
                }

                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
                    std::println("double clicked");
                    if(is_folder) {
                        folder_clicked = true;
                        clicked_path = current_path / item;
                    }
                }

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() / 2) - (ImGui::CalcTextSize(item.string().c_str()).x / 2));
                ImGui::TextWrapped("%s", item.string().c_str());
                ImGui::PopID();
                ImGui::NextColumn();
            }
        };

        draw_thumbnail(cached_folders, true);
        draw_thumbnail(cached_files, false);

        ImGui::EndChild();

        ImGui::End();

        if(folder_clicked) {
            update(clicked_path);
        }
    }
}
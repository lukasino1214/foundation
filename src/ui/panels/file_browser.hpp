#pragma once

#include <ui/ui_window.hpp>

namespace foundation {
    struct FileBrowser : public UIWindow {
    private:
        struct TreeNode {
            std::filesystem::path path = {};
            std::vector<TreeNode> children = {};
            bool is_open = false;
        };

    public:
        explicit FileBrowser(const std::filesystem::path& folder);

        void draw() override;
        auto should_close() -> bool override; 
        
    private:
        void update(const std::filesystem::path& path);

        std::filesystem::path selected_path = {};

        std::filesystem::path root_path = {};
        std::filesystem::path current_path = {};
        std::filesystem::file_time_type cached_time_stamp = {};

        std::vector<std::filesystem::path> cached_folders = {};
        std::vector<std::filesystem::path> cached_files = {};
    
        std::vector<TreeNode> tree_hiearchy = {};

        bool b_open = true;
    };
}
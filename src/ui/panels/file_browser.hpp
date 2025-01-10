#pragma once

namespace foundation {
    struct FileBrowser {
        struct TreeNode {
            std::filesystem::path path = {};
            std::vector<TreeNode> children = {};
            bool is_open = false;
        };

        FileBrowser();

        void update(const std::filesystem::path& path);

        void draw();

        std::filesystem::path selected_path = {};

        std::filesystem::path root_path = {};
        std::filesystem::path current_path = {};
        std::filesystem::file_time_type cached_time_stamp = {};

        std::vector<std::filesystem::path> cached_folders = {};
        std::vector<std::filesystem::path> cached_files = {};
    
        std::vector<TreeNode> tree_hiearchy = {};
    };
}
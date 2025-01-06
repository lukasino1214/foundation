#pragma once

namespace foundation {
    struct FileBrowser {
        FileBrowser();

        void draw();

        std::filesystem::path current_path = {};

        std::vector<std::filesystem::path> cached_folders = {};
        std::vector<std::filesystem::path> cached_files = {};
    };
}
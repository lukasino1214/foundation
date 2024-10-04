#pragma once

namespace foundation {
    struct FileWatcher {
        struct FileRecord {
            std::filesystem::file_time_type last_write;
        };

        std::unordered_map<std::filesystem::path, FileRecord> records = {};
        bool debug_print = true;

        void watch(const std::filesystem::path& path);
        void update(const std::function<void(const std::filesystem::path& path)>& fn);
    };
}
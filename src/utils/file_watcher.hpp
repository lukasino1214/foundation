#pragma once

namespace foundation {
    struct FileWatcher {
        // using Function = std::function<void(const std::filesystem::path& path, void* ptr)>;
        // struct FileType { u32 value; };

        struct FileRecord {
            std::filesystem::file_time_type last_write;
            // FileType type;
        };

        // struct FunctionRecord {
        //     Function function = {};
        //     void* ptr = {};
        // };

        std::unordered_map<std::filesystem::path, FileRecord> records = {};
        // std::unordered_map<u32, FunctionRecord> function_table = {};
        bool debug_print = true;

        // void register_function(FileType type, const Function& fn, void* user_data);
        void watch(const std::filesystem::path& path);
        void update(const std::function<void(const std::filesystem::path& path)>& fn);
    };
}
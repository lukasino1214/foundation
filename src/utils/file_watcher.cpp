#include <utils/file_watcher.hpp>

namespace foundation {
    void FileWatcher::watch(const std::filesystem::path& path) {
        if(std::filesystem::exists(path)) {
            std::filesystem::file_time_type last_write = std::filesystem::last_write_time(path);
            records.insert({path, FileRecord { .last_write = last_write }});
        }
    }

    void FileWatcher::update(const std::function<void(const std::filesystem::path& path)>& fn) {
        using namespace std::chrono_literals;
        static constexpr auto HOT_RELOAD_MIN_TIME = 250ms;

        auto now = std::chrono::file_clock::now();

        for(auto& [path, record] : records) {
            std::filesystem::file_time_type last_write = std::filesystem::last_write_time(path);
            if(now - last_write < HOT_RELOAD_MIN_TIME) { continue; }
            if(last_write > record.last_write) {
                if(debug_print) { std::println("{} was updated", path.string()); }
                record.last_write = last_write;
                fn(path);
            }
        }
    }
}
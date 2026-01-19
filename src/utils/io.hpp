#pragma once

namespace foundation {
    namespace io {
        auto read_file(const std::filesystem::path& file_path) -> std::vector<std::byte>;
    }
}
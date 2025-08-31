#pragma once

namespace foundation {
    void write_bytes_to_file(const std::vector<byte>& data, const std::filesystem::path& file_path);

    auto read_file_to_bytes(const std::filesystem::path& file_path) -> std::vector<std::byte>;
    auto read_file_to_string(const std::filesystem::path& file_path) -> std::string;
}

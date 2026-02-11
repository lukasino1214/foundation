#include "io.hpp"

#include <fstream>

namespace foundation {
    namespace io {
        auto read_file(const std::filesystem::path& file_path) -> std::vector<std::byte> {
            PROFILE_SCOPE;

            if(!std::filesystem::exists(file_path)) {
                throw std::runtime_error("file hasnt been found: " + file_path.string());
            }

            std::ifstream file(file_path, std::ios::binary | std::ios::ate);
            const auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<std::byte> data(s_cast<size_t>(size));

            if (!file.read(r_cast<char*>(data.data()), size)) {
                throw std::runtime_error("failed to read file: " + file_path.string());
            }

            return data;
        }
    }
}
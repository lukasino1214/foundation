#include "io.hpp"

#include <fstream>

namespace foundation {
    namespace io {
        auto read_file(const std::filesystem::path& file_path) -> std::vector<std::byte> {
            if(!std::filesystem::exists(file_path)) {
                throw std::runtime_error("file hasnt been found: " + file_path.string());
            }

            std::ifstream file(file_path, std::ios::binary);
            auto size = std::filesystem::file_size(file_path);
            std::vector<std::byte> data = {};
            data.resize(size);
            file.read(r_cast<char*>(data.data()), s_cast<std::streamsize>(size));

            return data;
        }
    }
}
#include <utils/file_io.hpp>

#include <fstream>

namespace foundation {
    void write_bytes_to_file(const std::vector<byte>& data, const std::filesystem::path& file_path) {
        std::ofstream file(file_path, std::ios_base::trunc | std::ios_base::binary);
        file.write(r_cast<char const *>(data.data()), s_cast<std::streamsize>(data.size()));
        file.close();
    }

    auto read_file_to_bytes(const std::filesystem::path& file_path) -> std::vector<std::byte> {
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

    auto read_file_to_string(const std::filesystem::path& file_path) -> std::string {
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error("file hasnt been found: " + file_path.string());
        }
    
        std::ifstream t(file_path.string());
        t.seekg(0, std::ios::end);
        usize size = s_cast<usize>(t.tellg());
        std::string buffer(size, ' ');
        t.seekg(0);
        t.read(buffer.data(), s_cast<std::streamsize>(size)); 
    
        return buffer;
    }
}
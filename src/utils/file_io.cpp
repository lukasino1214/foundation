#include <utils/file_io.hpp>

#include <fstream>

namespace foundation {
    auto load_file_to_string(const std::string_view& file_path) -> std::string {
        if(!std::filesystem::exists(file_path)) {
            throw std::runtime_error("file hasnt been found: " + std::string{file_path});
        }
    
        std::ifstream t(file_path.data());
        t.seekg(0, std::ios::end);
        size_t size = t.tellg();
        std::string buffer(size, ' ');
        t.seekg(0);
        t.read(buffer.data(), size); 
    
        return buffer;
    }
}
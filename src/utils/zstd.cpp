#include <utils/zstd.hpp>
#include <zstd.h>

namespace foundation {
    auto zstd_compress(const std::vector<byte>& data, u32 compression_level) -> std::vector<byte> {
        usize compressed_data_size = ZSTD_compressBound(data.size());
        std::vector<byte> compressed_data = {};
        compressed_data.resize(compressed_data_size);
        compressed_data_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), data.data(), data.size(), s_cast<i32>(compression_level));
        compressed_data.resize(compressed_data_size);
        return compressed_data;
    }

    auto zstd_decompress(const std::vector<byte>& data) -> std::vector<byte> {
        std::vector<std::byte> uncompressed_data = {};
        usize uncompressed_data_size = ZSTD_getFrameContentSize(data.data(), data.size());
        uncompressed_data.resize(uncompressed_data_size);
        uncompressed_data_size = ZSTD_decompress(uncompressed_data.data(), uncompressed_data.size(), data.data(), data.size());
        uncompressed_data.resize(uncompressed_data_size);
        return uncompressed_data;
    }
}
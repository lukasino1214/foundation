#pragma once

#include <pch.hpp>

namespace foundation {
    auto zstd_compress(const std::vector<byte>& data, u32 compression_level) -> std::vector<byte>;
    auto zstd_decompress(const std::vector<byte>& data) -> std::vector<byte>;
}
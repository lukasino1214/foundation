#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include <filesystem>
#include <functional>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <concepts>
#include <span>
#include <bit>
#include <print>
#include <type_traits>
#include <utility>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/mem.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define s_cast static_cast
#define d_cast dynamic_cast
#define r_cast reinterpret_cast

namespace Shaper {
    inline namespace types {
        using u8 = std::uint8_t;
        using u16 = std::uint16_t;
        using u32 = std::uint32_t;
        using u64 = std::uint64_t;
        using usize = std::size_t;
        using b32 = u32;

        using i8 = std::int8_t;
        using i16 = std::int16_t;
        using i32 = std::int32_t;
        using i64 = std::int64_t;
        using isize = std::ptrdiff_t;

        using f32 = float;
        using f64 = double;
    }
} // namespace Shaper::types

#include <shader_library/shared.inl>
#include <common/result.hpp>
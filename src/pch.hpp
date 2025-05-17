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
#include <thread>
#include <mutex>

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

template<typename T>
auto yeet_cast(auto&& value) {
    return *r_cast<T*>(&value);
}

#define y_cast yeet_cast

#include <tracy/Tracy.hpp>

namespace foundation {
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

        using byte = std::byte;
    }
}

#include <shader_library/shared.inl>
#include <common/result.hpp>

inline constexpr auto find_msb(daxa::u32 v) -> daxa::u32 {
    daxa::u32 index = 0;
    while (v != 0) {
        v = v >> 1;
        index = index + 1;
    }
    return index;
}

inline constexpr auto find_next_lower_po2(daxa::u32 v) -> daxa::u32 {
    auto const msb = find_msb(v);
    return 1u << ((msb == 0 ? 1 : msb) - 1);
}

// #define PROFILE_ON
#if defined(PROFILE_ON)
#define PROFILE_FRAME_START(name) FrameMarkStart(name)
#define PROFILE_FRAME_END(name) FrameMarkEnd(name)
#define PROFILE_SCOPE ZoneScoped
#define PROFILE_SCOPE_NAMED(name) ZoneNamedN(name, #name, true)
#define PROFILE_ZONE_NAMED(name) ZoneTransientN(name, #name, true)
#else
#define PROFILE_FRAME_START(name)
#define PROFILE_FRAME_END(name)
#define PROFILE_SCOPE
#define PROFILE_SCOPE_NAMED(name)
#define PROFILE_ZONE_NAMED(name)
#endif

#define LOG_ON
#include <utils/logger.hpp>
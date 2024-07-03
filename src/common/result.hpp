#pragma once

// #include <pch.hpp>

// namespace Shaper {
//     template<typename T>
//     concept enum_u32 = std::is_enum_v<T> && 
//                        std::is_same_v<std::underlying_type_t<T>, u32> && 
//                        requires{ { T::Success } -> std::convertible_to<T>; };
    
//     template<typename T, enum_u32 E>
//     struct Result {
//         std::optional<T> v = {};
//         E error_code = {};
//         std::string m = {};

//         explicit Result(T && value) : v{std::move(value)}, m{""} {}
//         explicit Result(T const & value) : v{value}, m{""} {}
//         explicit Result(std::optional<T> && opt) : v{std::move(opt)}, m{opt.has_value() ? "" : "default error message"} {}
//         explicit Result(std::optional<T> const & opt) : v{opt}, m{opt.has_value() ? "" : "default error message"} {}
//         explicit Result(E code, const std::string& message) : v{std::nullopt}, error_code{code}, m{message} {}

//         bool is_ok() const { return v.has_value(); }
//         bool is_err() const { return !v.has_value(); }
//         auto value() const -> T const & { return v.value(); }
        
//         auto value() -> T & {
//             if(!v.has_value()) { throw std::runtime_error(m != "" ? m : "tried getting value of empty Result"); }
//             return v.value();
//         }

//         auto message() const -> std::string const & {
//             return m;
//         }

//         auto to_string() const -> std::string {
//             if (v.has_value()) {
//                 return "Result OK";
//             } else {
//                 return std::string("Result Err: ") + m;
//             }
//         }

//         operator bool() const { return v.has_value(); }
//         auto operator!() const -> bool { return !v.has_value(); }
//     }
// };
#pragma once

#include <pch.hpp>

namespace foundation {

    struct ByteReader;
    template<typename T>
    concept HasDeserialize = requires(ByteReader& r) { { T::deserialize(r) } -> std::same_as<T>; };
    
    struct ByteWriter;
    template<typename T>
    concept HasSerialize = requires(ByteWriter& w, const T& v) { { T::serialize(w, v) } -> std::same_as<void>; };

    struct ByteReader {
        std::byte* data = {};
        usize size = {};
        usize offset = 0;

        auto read_raw(usize _size) -> std::byte* {
            assert(offset + _size <= size);
            std::byte* ptr = data + offset;
            offset += _size;
            return ptr;
        }

        template<typename T>
        auto read() -> T {
            if constexpr(std::is_same_v<T, std::string>) {
                return read_string();
            }

            if constexpr(HasDeserialize<T>) {
                return T::deserialize(*this);
            }

            return *r_cast<T*>(read_raw(sizeof(T)));
        }

        template<typename T>
        auto read(T& value) {
            value = read<T>();
        }

        auto read_string() -> std::string {
            assert(offset + sizeof(u32) <= size);
            u32 _size = read<u32>();
            char* ptr = r_cast<char*>(read_raw(s_cast<usize>(_size)));
            return std::string{ptr, _size};
        }

    };

    struct ByteWriter {
        std::vector<std::byte> data = {};

        template<typename T>
        void write_raw(T* value, usize size) {
            usize offset = data.size();
            data.resize(offset + size);
            std::memcpy(data.data() + offset, value, size);
        }

        template<typename T>
        void write(const T& value) {
            if constexpr (std::is_same_v<T, std::string>) {
                write_string(value);
                return;
            }

            if constexpr(HasSerialize<T>) {
                T::serialize(*this, value);
                return; 
            }

            write_raw(&value, sizeof(T));
        }

        void write_string(const std::string& value) {
            write(s_cast<u32>(value.size()));
            write_raw(value.data(), value.size());
        }
    };
}
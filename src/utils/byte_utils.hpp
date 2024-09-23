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

        template<typename T>
        auto read(T& value) {
            value = read<T>();
        }

        template<typename T>
        auto read() -> T {
            return *r_cast<T*>(read_raw(sizeof(T)));
        }

        template<HasSerialize T>
        void read(T& value) {
            value = T::deserialize(*this);
        }

        template<typename T>
        void read(std::vector<T>& value) {
            assert(offset + sizeof(u32) <= size);
            u32 vec_size = read<u32>();
            value.resize(vec_size);
            u32 byte_size = vec_size * sizeof(T);
            char* ptr = r_cast<char*>(read_raw(s_cast<usize>(byte_size)));
            std::memcpy(value.data(), ptr, byte_size);
        }

        void read(std::string& value) {
            assert(offset + sizeof(u32) <= size);
            u32 _size = read<u32>();
            char* ptr = r_cast<char*>(read_raw(s_cast<usize>(_size)));
            value = std::string{ptr, _size};
        }

    private:
        auto read_raw(usize _size) -> std::byte* {
            assert(offset + _size <= size);
            std::byte* ptr = data + offset;
            offset += _size;
            return ptr;
        }
    };

    struct ByteWriter {
        std::vector<std::byte> data = {};

        void write(const void* value, usize size) {
            usize offset = data.size();
            data.resize(offset + size);
            std::memcpy(data.data() + offset, value, size);
        }

        template<typename T>
        void write(const T& value) {
            write(&value, sizeof(T));
        }

        template<HasSerialize T>
        void write(const T& value) {
            T::serialize(*this, value);
        }

        template<typename T>
        void write(const std::vector<T>& value) {
            write(s_cast<u32>(value.size()));
            write(value.data(), value.size() * sizeof(T));
        }

        void write(const std::string& value) {
            write(s_cast<u32>(value.size()));
            write(value.data(), value.size());
        }
    };
}
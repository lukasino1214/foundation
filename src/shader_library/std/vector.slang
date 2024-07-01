#pragma once

#include "../../shared/shared.inl"
#include "allocator.slang"
#include "memory_ops.slang"

namespace std {
    struct vector<T, let n: u32 = 4> {
        T* data;
        Allocator* allocator;
        u32 _size;
        u32 _capacity;

        __init(Allocator* _allocator) {
            allocator = _allocator;
            data = (T*)allocator.allocate<T>();
            _size = 0;
            _capacity = n;
        }

        __subscript(u32 index) -> T {
            get { return data[index]; }
            set { data[index] = newValue; }
        }

        fn at(u32 index) -> T {
            return data[index];
        }
    
        fn size() -> u32 {
            return _size;
        }

        fn capacity() -> u32 {
            return _capacity;
        }

        fn front() -> T {
            return data[0];
        }

        fn back() -> T {
            return data[_size - 1];
        }

        [mutating]
        fn fill(T value) {
            for(u32 i = 0; i < _size; i++) {
                data[i] = value;
            }
        }

        [mutating]
        fn clear() {
            _size = 0;
        }

        fn empty() -> u32 {
            return _size == 0;
        }
        
        [mutating]
        fn push_back(T value) {
            if (_size == _capacity) {
                resize(_capacity * 2);
            }
            data[_size++] = value;
        }

        [mutating]
        fn pop_back() {
            if (_size > 0) {
                _size--;
            }
        }

        [mutating]
        fn resize(u32 new_capacity) {
            T* new_data = allocator.allocate<T>();
            memcpy(reinterpret<Ptr<void>>(new_data), reinterpret<Ptr<void>>(data), _size);
            allocator.deallocate<T>(data);
            data = new_data;
            _capacity = new_capacity;
        }
    }
}
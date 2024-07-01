#pragma once

#include "../../shared/shared.inl"

namespace std {
    struct array<T, let N: u32> {
        T data[N];
    
        __subscript(u32 index) -> T {
            get { return data[index]; }
            set { data[index] = newValue; }
        }

        fn at(u32 index) -> T {
            return data[index];
        }
    
        fn size() -> u32 {
            return N;
        }

        fn front() -> T {
            return data[0];
        }

        fn back() -> T {
            return data[N - 1];
        }

        [mutating]
        fn fill(T value) {
            for(u32 i = 0; i < N; i++) {
                data[i] = value;
            }
        }
    }
}
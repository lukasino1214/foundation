#pragma once

#include "../../shared/shared.inl"

namespace std {
    struct Allocator {
    
        fn allocate<T>() -> T* {
            return (T*)0;
        }

        fn deallocate<T>(T* ptr) {

        }
    }
}
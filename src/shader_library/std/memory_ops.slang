#pragma once

#include "../../shared/shared.inl"

namespace std {
    fn memcpy(void* dest, void* src, u32 size) {
        for(u32 i = 0; i < size; i++) {
            dest[i] = src[i];
        }
    }
    
    fn memmove(void* dest, void* src, u32 size) {
        if (dest > src && dest < src + size) {
            for (size_t i = size; i != 0; i--) {
                dest[i-1] = src[i-1];
            }
        } else {
            for (size_t i = 0; i < size; i++) {
                dest[i] = src[i];
            }
        }
    }
    
    fn memset(void* dest, u32 value, u32 size) {
        u32 new_size = size / 4;
        u32* dest_ptr = (u32*)dest;
    
        for (size_t i = 0; i < new_size; i++) {
            dest_ptr[i] = value;
        }
    }
}
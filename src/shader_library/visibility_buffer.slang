#pragma once
#include "glue.inl"
#include <daxa/daxa.inl>

[[vk::binding(DAXA_STORAGE_IMAGE_BINDING, 0)]] RWTexture2D<daxa::u64> tex_rw_u64_table[];

[ForceInline]
func AtomicMaxU64(__ref u64 dest, u64 value) -> u64 {
    u64 original_value;
    spirv_asm {
        OpCapability Int64Atomics;
        %origin:$$u64 = OpAtomicUMax &dest Device None $value;
        OpStore &original_value %origin
    };
    return original_value;
}

func encode_visibility_buffer_payload(u32 meshlet_data_index, f32 depth) -> u64 {
    const u64 u64_depth = u64(asuint(depth));
    return (u64_depth << 32) | meshlet_data_index;
}

func decode_visibility_buffer_payload(u64 payload, __ref u32 meshlet_data_index, __ref u32 depth) {
    meshlet_data_index = daxa_u32(payload & (~(daxa_u32(0))));
    depth = daxa_u32(payload >> 32);
}

func decode_visibility_buffer_payload(u64 payload, __ref u32 meshlet_data_index, __ref f32 depth) {
    meshlet_data_index = daxa_u32(payload & (~(daxa_u32(0))));
    depth = asfloat(daxa_u32(payload >> 32));
}
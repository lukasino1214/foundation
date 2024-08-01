#pragma once
#include "../../shader_library/shared.inl"

fn normal_mapping(f32vec3 sampled_normal, f32vec3 tangent, f32vec3 bittangent, f32vec3 normal) -> f32vec3 {
    daxa_f32mat3x3 tbn = daxa_f32mat3x3(tangent, bittangent, normal);
    return normalize(mul(tbn, sampled_normal));
}
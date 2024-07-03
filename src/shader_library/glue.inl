#pragma once

#if defined(__cplusplus)

typedef glm::vec2 f32vec2;
typedef glm::vec3 f32vec3;
typedef glm::vec4 f32vec4;
typedef glm::u32vec3 u32vec3;
typedef daxa_u32 u32;

#define fn auto

#elif DAXA_SHADERLANG != DAXA_SHADERLANG_GLSL

typedef daxa_u32 u32;
typedef daxa_f32 f32;

typedef daxa_f32vec2 f32vec2;
typedef daxa_f32vec3 f32vec3;
typedef daxa_f32vec4 f32vec4;
typedef daxa_u32vec3 u32vec3;

daxa_f32vec4 operator* (daxa_f32mat4x4 a, daxa_f32vec4 b) {
    return mul(a, b);
}

#define fn func

#endif
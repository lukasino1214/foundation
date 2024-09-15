#pragma once

#if defined(__cplusplus)

using f32mat4x4 = glm::mat4x4;

using f32vec4 = glm::vec4;
using f32vec3 = glm::vec3;
using f32vec2 = glm::vec2;

using i32vec4 = glm::ivec4;
using i32vec3 = glm::ivec3;
using i32vec2 = glm::ivec2;

using u32vec4 = glm::uvec4;
using u32vec3 = glm::uvec3;
using u32vec2 = glm::uvec2;

using f32 = daxa_f32;
using i32 = daxa_i32;
using u32 = daxa_u32;
using i64 = daxa_i64;
using u64 = daxa_u64;
using b32 = daxa_b32;

#define const_ref(x__) const x__ & 
#define pack_half_2x16 glm::packHalf2x16
#define unpack_half_2x16 glm::unpackHalf2x16
#define pack_snorm_2x16 glm::packSnorm2x16 
#define unpack_snorm_2x16 glm::unpackSnorm2x16 

template<typename T>
constexpr auto max(const T& a, const T& b) -> T {
    return glm::max(a, b);
}

template<typename T>
constexpr auto min(const T& a, const T& b) -> T {
    return glm::min(a, b);
}

#elif DAXA_SHADERLANG == DAXA_SHADERLANG_SLANG

typedef daxa_f32mat4x4 f32mat4x4;

typedef daxa_f32vec4 f32vec4;
typedef daxa_f32vec3 f32vec3;
typedef daxa_f32vec2 f32vec2;

typedef daxa_i32vec4 i32vec4;
typedef daxa_i32vec3 i32vec3;
typedef daxa_i32vec2 i32vec2;

typedef daxa_u32vec4 u32vec4;
typedef daxa_u32vec3 u32vec3;
typedef daxa_u32vec2 u32vec2;

typedef daxa_f32 f32;
typedef daxa_i32 i32;
typedef daxa_u32 u32;
typedef daxa_i64 i64;
typedef daxa_u64 u64;
typedef daxa_b32 b32;

#define auto func
#define const_ref(x__) x__

[__readNone]
[ForceInline]
[require(cpp_cuda_glsl_hlsl_spirv, shader5_sm_4_0)]
auto pack_half_2x16(f32vec2 v) -> u32 {
    return spirv_asm {
        result:$$u32 = OpExtInst glsl450 PackHalf2x16 $v
    };
}

[__readNone]
[ForceInline]
[require(cpp_cuda_glsl_hlsl_spirv, shader5_sm_4_0)]
auto unpack_half_2x16(u32 v) -> f32vec2 {
    return spirv_asm {
        result:$$f32vec2 = OpExtInst glsl450 UnpackHalf2x16 $v
    };
}

[__readNone]
[ForceInline]
[require(cpp_cuda_glsl_hlsl_spirv, shader5_sm_4_0)]
auto pack_snorm_2x16(f32vec2 v) -> u32 {
    return spirv_asm {
        result:$$u32 = OpExtInst glsl450 PackSnorm2x16 $v
    };
}

[__readNone]
[ForceInline]
[require(cpp_cuda_glsl_hlsl_spirv, shader5_sm_4_0)]
auto unpack_snorm_2x16(u32 v) -> f32vec2 {
    return spirv_asm {
        result:$$f32vec2 = OpExtInst glsl450 UnpackSnorm2x16 $v
    };
}

#elif DAXA_SHADERLANG == DAXA_SHADERLANG_GLSL

#define f32mat4x4 daxa_f32mat4x4

#define f32vec4 daxa_f32vec4
#define f32vec3 daxa_f32vec3
#define f32vec2 daxa_f32vec2

#define i32vec4 daxa_i32vec4
#define i32vec3 daxa_i32vec3
#define i32vec2 daxa_i32vec2

#define u32vec4 daxa_u32vec4
#define u32vec3 daxa_u32vec3
#define u32vec2 daxa_u32vec2

#define f32 daxa_f32
#define i32 daxa_i32
#define u32 daxa_u32
#define i64 daxa_i64
#define u64 daxa_u64
#define b32 daxa_b32

#define pack_half_2x16 packHalf2x16
#define unpack_half_2x16 unpackHalf2x16
#define pack_snorm_2x16 packSnorm2x16 
#define unpack_snorm_2x16 unpackSnorm2x16 

#endif
#pragma once
#include <daxa/daxa.inl>
#include "glue.inl"

struct ShaderGlobals {
    daxa_f32mat4x4 camera_projection_matrix;
    daxa_f32mat4x4 camera_inverse_projection_matrix;
    daxa_f32mat4x4 camera_view_matrix;
    daxa_f32mat4x4 camera_inverse_view_matrix;
    daxa_f32mat4x4 camera_projection_view_matrix;
    daxa_f32mat4x4 camera_inverse_projection_view_matrix;
    daxa_f32vec4 frustum_planes[6];
    daxa_u32vec2 resolution;
    daxa_f32vec3 camera_position;
    daxa_SamplerId linear_sampler;
    daxa_SamplerId nearest_sampler;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

struct TransformInfo {
    daxa_f32mat4x4 model_matrix;
    daxa_f32mat4x4 normal_matrix;
};

DAXA_DECL_BUFFER_PTR(TransformInfo)

struct Vertex {
    daxa_f32vec3 position;
    daxa_f32vec2 uv;
};

DAXA_DECL_BUFFER_PTR(Vertex)

struct Material {
    daxa_ImageViewId albedo_texture_id;
    daxa_SamplerId albedo_sampler_id;
    daxa_ImageViewId normal_texture_id;
    daxa_SamplerId normal_sampler_id;
    daxa_ImageViewId roughness_metalness_texture_id;
    daxa_SamplerId roughness_metalness_sampler_id;
    daxa_ImageViewId emissive_texture_id;
    daxa_SamplerId emissive_sampler_id;
};

DAXA_DECL_BUFFER_PTR(Material)

#if defined(__cplusplus)
#define SHARED_FUNCTION inline
#define SHARED_FUNCTION_INOUT(X) X &
#else
#define SHARED_FUNCTION
#define SHARED_FUNCTION_INOUT(X) inout X
#endif

SHARED_FUNCTION daxa_u32 round_up_to_multiple(daxa_u32 value, daxa_u32 multiple_of) {
    return ((value + multiple_of - 1) / multiple_of) * multiple_of;
}

SHARED_FUNCTION daxa_u32 round_up_div(daxa_u32 value, daxa_u32 div) {
    return (value + div - 1) / div;
}
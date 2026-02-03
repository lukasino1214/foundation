#pragma once
#include <daxa/daxa.inl>
#include "glue.inl"

#define INVALID_ID (~(0u))

struct CameraInfo {
    f32mat4x4 projection_matrix;
    f32mat4x4 inverse_projection_matrix;
    f32mat4x4 view_matrix;
    f32mat4x4 inverse_view_matrix;
    f32mat4x4 projection_view_matrix;
    f32mat4x4 inverse_projection_view_matrix;
    f32vec4 frustum_planes[6];
    f32vec3 position;
    f32 vertical_fov;
};
DAXA_DECL_BUFFER_PTR(CameraInfo)

struct ShaderGlobals {
    CameraInfo main_camera;
    u32vec2 render_target_size;
    f32vec2 render_target_size_inv;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

struct MeshGeometryData {
    daxa_BufferPtr(f32vec3) positions;
    daxa_BufferPtr(f32vec3) normals;
    daxa_BufferPtr(f32vec2) uvs;
    daxa_BufferPtr(u32) indices;
    u32 material_index;
    u32 vertex_count;
    u32 index_count;
};

DAXA_DECL_BUFFER_PTR(MeshGeometryData)

struct Material {
    daxa_ImageViewId albedo_image_id;
    daxa_SamplerId albedo_sampler_id;
    // daxa_ImageViewId alpha_mask_image_id;
    // daxa_SamplerId alpha_mask_sampler_id;
    daxa_ImageViewId normal_image_id;
    daxa_SamplerId normal_sampler_id;
    daxa_ImageViewId roughness_metalness_image_id;
    daxa_SamplerId roughness_metalness_sampler_id;
    // daxa_ImageViewId roughness_image_id;
    // daxa_SamplerId roughness_sampler_id;
    // daxa_ImageViewId metalness_image_id;
    // daxa_SamplerId metalness_sampler_id;
    daxa_ImageViewId emissive_image_id;
    daxa_SamplerId emissive_sampler_id;
    f32vec4 albedo_factor;
    f32 metallic_factor;
    f32 roughness_factor;
    f32vec3 emissive_factor;
    u32 alpha_mode;
    f32 alpha_cutoff;
    b32 double_sided;
};

DAXA_DECL_BUFFER_PTR(GPUMaterial)

#if defined(__cplusplus)
#define SHARED_FUNCTION inline
#define SHARED_FUNCTION_INOUT(X) X &
#else
#define SHARED_FUNCTION
#define SHARED_FUNCTION_INOUT(X) inout X
#endif

SHARED_FUNCTION u32 round_up_to_multiple(u32 value, u32 multiple_of) {
    return ((value + multiple_of - 1) / multiple_of) * multiple_of;
}

SHARED_FUNCTION u32 round_up_div(u32 value, u32 div) {
    return (value + div - 1) / div;
}

SHARED_FUNCTION f32mat4x4 mat_4x3_to_4x4(f32mat4x3 mat) {
#if DAXA_LANGUAGE == DAXA_LANGUAGE_SLANG
    f32mat4x4 ret = daxa_f32mat4x4(
        f32vec4(mat[0]),
        f32vec4(mat[1]),
        f32vec4(mat[2]),
        f32vec4(0.0f,0.0f,0.0f, 1.0f));
#else
    f32mat4x4 ret = f32mat4x4(
        f32vec4(mat[0], 0.0),
        f32vec4(mat[1], 0.0),
        f32vec4(mat[2], 0.0),
        f32vec4(mat[3], 1.0));
#endif
    return ret;
}
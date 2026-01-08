#pragma once
#define DAXA_RAY_TRACING 1
#include <daxa/daxa.inl>
#include "glue.inl"

#define INVALID_ID (~(0u))

struct DrawIndexedIndirectStruct {
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    u32 vertex_offset;
    u32 first_instance;
};
DAXA_DECL_BUFFER_PTR(DrawIndexedIndirectStruct)

struct DrawIndirectStruct {
    u32 vertex_count;
    u32 instance_count;
    u32 first_vertex;
    u32 first_instance;
};
DAXA_DECL_BUFFER_PTR(DrawIndirectStruct)

struct DispatchIndirectStruct {
    u32 x;
    u32 y;
    u32 z;
};
DAXA_DECL_BUFFER_PTR(DispatchIndirectStruct)

struct ShaderDebugAABBDraw {
    f32vec3 color;
    f32vec3 position;
    f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugAABBDraw)

struct ShaderDebugCircleDraw {
    f32vec3 color;
    f32vec3 position;
    f32 radius;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugCircleDraw)

struct ShaderDebugLineDraw {
    f32vec3 color;
    f32vec3 start;
    f32vec3 end;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugLineDraw)

struct ShaderDebugBufferHead {
    DrawIndirectStruct aabb_draw_indirect_info;
    DrawIndirectStruct circle_draw_indirect_info;
    DrawIndirectStruct line_draw_indirect_info;
    daxa_RWBufferPtr(ShaderDebugAABBDraw) aabb_draws;
    daxa_RWBufferPtr(ShaderDebugCircleDraw) circle_draws;
    daxa_RWBufferPtr(ShaderDebugLineDraw) line_draws;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugBufferHead)

struct CameraInfo {
    f32mat4x4 projection_matrix;
    f32mat4x4 inverse_projection_matrix;
    f32mat4x4 view_matrix;
    f32mat4x4 inverse_view_matrix;
    f32mat4x4 projection_view_matrix;
    f32mat4x4 inverse_projection_view_matrix;
    f32vec4 frustum_planes[6];
    f32vec3 position;
};
DAXA_DECL_BUFFER_PTR(CameraInfo)

struct ShaderGlobals {
    CameraInfo main_camera;
    CameraInfo observer_camera;
    b32 render_as_observer;
    u32vec2 render_target_size;
    f32vec2 render_target_size_inv;
    daxa_BufferPtr(ShaderDebugBufferHead) debug;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

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

// cringe(note: glsl is column major and slang/hlsl is row major for indexing but multiplication is column major, dont ask why)
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
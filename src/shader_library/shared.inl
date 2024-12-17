#pragma once
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

struct ShaderDebugEntityOOBDraw {
    f32vec3 color;
    u32 transform_index;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugEntityOOBDraw)

struct ShaderDebugAABBDraw {
    f32vec3 color;
    f32vec3 position;
    f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugAABBDraw)

struct ShaderDebugBufferHead {
    DrawIndirectStruct entity_oob_draw_indirect_info;
    DrawIndirectStruct aabb_draw_indirect_info;
    daxa_RWBufferPtr(ShaderDebugEntityOOBDraw) entity_oob_draws;
    daxa_RWBufferPtr(ShaderDebugAABBDraw) aabb_draws;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugBufferHead)

struct Samplers {
    daxa_SamplerId linear_clamp;
    daxa_SamplerId linear_repeat;
    daxa_SamplerId nearest_repeat;
    daxa_SamplerId nearest_clamp;
    daxa_SamplerId linear_repeat_anisotropy;
    daxa_SamplerId nearest_repeat_anisotropy;
};
DAXA_DECL_BUFFER_PTR(Samplers)

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
    u32vec2 next_lower_po2_render_target_size;
    f32vec2 next_lower_po2_render_target_size_inv;
    Samplers samplers;
    daxa_BufferPtr(ShaderDebugBufferHead) debug;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

struct TransformInfo {
    f32mat4x3 model_matrix;
};

DAXA_DECL_BUFFER_PTR(TransformInfo)

struct Material {
    daxa_ImageViewId albedo_image_id;
    daxa_SamplerId albedo_sampler_id;
    daxa_ImageViewId alpha_mask_image_id;
    daxa_SamplerId alpha_mask_sampler_id;
    daxa_ImageViewId normal_image_id;
    daxa_SamplerId normal_sampler_id;
    daxa_ImageViewId roughness_image_id;
    daxa_SamplerId roughness_sampler_id;
    daxa_ImageViewId metalness_image_id;
    daxa_SamplerId metalness_sampler_id;
    daxa_ImageViewId emissive_image_id;
    daxa_SamplerId emissive_sampler_id;
    f32 metallic_factor;
    f32 roughness_factor;
    f32vec3 emissive_factor;
    u32 alpha_mode;
    f32 alpha_cutoff;
    b32 double_sided;
};

DAXA_DECL_BUFFER_PTR(Material)

struct GPUSceneData {
    u32 entity_count;
};

DAXA_DECL_BUFFER_PTR(GPUSceneData)

struct MeshGroup {
    daxa_BufferPtr(u32) mesh_indices;
    u32 count;
    u32 padding;
};

DAXA_DECL_BUFFER_PTR_ALIGN(MeshGroup, 8)

#define MAX_VERTICES_PER_MESHLET (64)
#define MAX_TRIANGLES_PER_MESHLET (64)

struct Meshlet {
    u32 indirect_vertex_offset;
    u32 micro_indices_offset;
    u32 vertex_count;
    u32 triangle_count;
};
DAXA_DECL_BUFFER_PTR(Meshlet)

struct BoundingSphere {
    f32vec3 center;
    f32 radius;
};
DAXA_DECL_BUFFER_PTR(BoundingSphere)

struct MeshletBoundingSpheres {
    BoundingSphere culling_sphere;
    BoundingSphere lod_group_sphere;
    BoundingSphere lod_parent_group_sphere;
};
DAXA_DECL_BUFFER_PTR(MeshletBoundingSpheres)

struct AABB {
    f32vec3 center;
    f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(AABB)

struct MeshletSimplificationError {
    f32 group_error;
    f32 parent_group_error;
};
DAXA_DECL_BUFFER_PTR(MeshletSimplificationError)

struct Mesh {
    daxa_BufferId mesh_buffer;
    u32 material_index;
    u32 meshlet_count;
    u32 vertex_count;
    AABB aabb;
    daxa_BufferPtr(Meshlet) meshlets;
    daxa_BufferPtr(MeshletBoundingSpheres) bounding_spheres;
    daxa_BufferPtr(MeshletSimplificationError) simplification_errors;
    daxa_BufferPtr(AABB) meshlet_aabbs;
    daxa_BufferPtr(u32) micro_indices;
    daxa_BufferPtr(u32) indirect_vertices;
    daxa_BufferPtr(u32) primitive_indices;
    daxa_BufferPtr(f32vec3) vertex_positions;
    daxa_BufferPtr(u32) vertex_normals;
    daxa_BufferPtr(u32) vertex_uvs;
};

DAXA_DECL_BUFFER_PTR_ALIGN(Mesh, 8)

struct MeshletData {
    u32 mesh_index;
    u32 mesh_group_index;
    u32 meshlet_index;
    u32 transform_index;
};
DAXA_DECL_BUFFER_PTR(MeshletData)

struct EntityData {
    u32 mesh_group_index;
    u32 transform_index;
};
DAXA_DECL_BUFFER_PTR(EntityData)

struct MeshData {
    u32 mesh_group_index;
    u32 local_mesh_index;
    u32 global_mesh_index;
    u32 transform_index;
};
DAXA_DECL_BUFFER_PTR(MeshData)

struct MeshesData {
    u32 count;
    daxa_BufferPtr(MeshData) meshes;
};
DAXA_DECL_BUFFER_PTR(MeshesData)

struct MeshletsData {
    u32 count;
    daxa_BufferPtr(MeshletData) meshlets;
};
DAXA_DECL_BUFFER_PTR(MeshletsData)

struct MeshletIndices {
    u32 count;
    daxa_BufferPtr(u32) indices;
};
DAXA_DECL_BUFFER_PTR(MeshletIndices)

struct MeshletIndexBuffer {
    u32 count;
    daxa_BufferPtr(u32) indices;
};
DAXA_DECL_BUFFER_PTR(MeshletIndexBuffer)

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

#define OVERDRAW_DEBUG 0
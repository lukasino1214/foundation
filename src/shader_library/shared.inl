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

struct ShaderDebugAABBDraw {
    f32vec3 color;
    u32 transform_index;
};
DAXA_DECL_BUFFER_PTR(ShaderDebugAABBDraw)

struct ShaderDebugBufferHead {
    DrawIndirectStruct aabb_draw_indirect_info;
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

struct ShaderGlobals {
    f32mat4x4 camera_projection_matrix;
    f32mat4x4 camera_inverse_projection_matrix;
    f32mat4x4 camera_view_matrix;
    f32mat4x4 camera_inverse_view_matrix;
    f32mat4x4 camera_projection_view_matrix;
    f32mat4x4 camera_inverse_projection_view_matrix;
    f32vec4 frustum_planes[6];
    u32vec2 render_target_size;
    f32vec2 render_target_size_inv;
    u32vec2 next_lower_po2_render_target_size;
    f32vec2 next_lower_po2_render_target_size_inv;
    f32vec3 camera_position;
    Samplers samplers;
    daxa_BufferPtr(ShaderDebugBufferHead) debug;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

struct TransformInfo {
    f32mat4x4 model_matrix;
    f32mat4x4 normal_matrix;
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

#define MAX_VERTICES_PER_MESHLET (124)
#define MAX_TRIANGLES_PER_MESHLET (124)

#define MAX_MESHES (1u << 10u)
#define MAX_SURVIVING_MESHLETS 1000000
#define MAX_MATERIALS (1u << 10u)

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

struct AABB {
    f32vec3 center;
    f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(AABB)

struct Mesh {
    u32 material_index;
    u32 meshlet_count;
    u32 vertex_count;
    AABB aabb;
    daxa_BufferPtr(Meshlet) meshlets;
    daxa_BufferPtr(BoundingSphere) meshlet_bounds;
    daxa_BufferPtr(AABB) meshlet_aabbs;
    daxa_BufferPtr(u32) micro_indices;
    daxa_BufferPtr(u32) indirect_vertices;
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

SHARED_FUNCTION daxa_u32 round_up_to_multiple(daxa_u32 value, daxa_u32 multiple_of) {
    return ((value + multiple_of - 1) / multiple_of) * multiple_of;
}

SHARED_FUNCTION daxa_u32 round_up_div(daxa_u32 value, daxa_u32 div) {
    return (value + div - 1) / div;
}
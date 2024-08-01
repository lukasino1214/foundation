#pragma once
#include <daxa/daxa.inl>
#include "glue.inl"

struct DrawIndexedIndirectStruct {
    daxa_u32 index_count;
    daxa_u32 instance_count;
    daxa_u32 first_index;
    daxa_u32 vertex_offset;
    daxa_u32 first_instance;
};
DAXA_DECL_BUFFER_PTR(DrawIndexedIndirectStruct)

struct DrawIndirectStruct {
    daxa_u32 vertex_count;
    daxa_u32 instance_count;
    daxa_u32 first_vertex;
    daxa_u32 first_instance;
};
DAXA_DECL_BUFFER_PTR(DrawIndirectStruct)

struct DispatchIndirectStruct {
    daxa_u32 x;
    daxa_u32 y;
    daxa_u32 z;
};
DAXA_DECL_BUFFER_PTR(DispatchIndirectStruct)

struct ShaderDebugAABBDraw {
    daxa_f32vec3 position;
    daxa_f32vec3 size;
    daxa_f32vec3 color;
    daxa_u32 coord_space;
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
    daxa_f32mat4x4 camera_projection_matrix;
    daxa_f32mat4x4 camera_inverse_projection_matrix;
    daxa_f32mat4x4 camera_view_matrix;
    daxa_f32mat4x4 camera_inverse_view_matrix;
    daxa_f32mat4x4 camera_projection_view_matrix;
    daxa_f32mat4x4 camera_inverse_projection_view_matrix;
    daxa_f32vec4 frustum_planes[6];
    daxa_u32vec2 render_target_size;
    daxa_f32vec2 render_target_size_inv;
    daxa_u32vec2 next_lower_po2_render_target_size;
    daxa_f32vec2 next_lower_po2_render_target_size_inv;
    daxa_f32vec3 camera_position;
    Samplers samplers;
    daxa_BufferPtr(ShaderDebugBufferHead) debug;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

struct TransformInfo {
    daxa_f32mat4x4 model_matrix;
    daxa_f32mat4x4 normal_matrix;
};

DAXA_DECL_BUFFER_PTR(TransformInfo)

struct Vertex {
    daxa_f32vec3 position;
    daxa_f32vec3 normal;
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
    daxa_f32 metallic_factor;
    daxa_f32 roughness_factor;
    daxa_f32vec3 emissive_factor;
    daxa_u32 alpha_mode;
    daxa_f32 alpha_cutoff;
    daxa_b32 double_sided;
};

DAXA_DECL_BUFFER_PTR(Material)

struct SceneData {
    daxa_u32 mesh_groups_count;
};

DAXA_DECL_BUFFER_PTR(SceneData)

struct MeshGroup {
    daxa_BufferPtr(daxa_u32) mesh_indices;
    daxa_u32 count;
    daxa_u32 padding;
};

DAXA_DECL_BUFFER_PTR_ALIGN(MeshGroup, 8)

#define MAX_VERTICES_PER_MESHLET (124)
#define MAX_TRIANGLES_PER_MESHLET (124)

#define MAX_MESHES (1u << 10u)
#define MAX_SURVIVING_MESHLETS 1000000
#define MAX_MATERIALS (1u << 10u)

struct Meshlet {
    daxa_u32 indirect_vertex_offset;
    daxa_u32 micro_indices_offset;
    daxa_u32 vertex_count;
    daxa_u32 triangle_count;
};
DAXA_DECL_BUFFER_PTR(Meshlet)

struct BoundingSphere {
    daxa_f32vec3 center;
    daxa_f32 radius;
};
DAXA_DECL_BUFFER_PTR(BoundingSphere)

struct AABB {
    daxa_f32vec3 center;
    daxa_f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(AABB)

struct Mesh {
    daxa_u32 material_index;
    daxa_u32 meshlet_count;
    daxa_u32 vertex_count;
    daxa_BufferPtr(Meshlet) meshlets;
    daxa_BufferPtr(BoundingSphere) meshlet_bounds;
    daxa_BufferPtr(AABB) meshlet_aabbs;
    daxa_BufferPtr(daxa_u32) micro_indices;
    daxa_BufferPtr(daxa_u32) indirect_vertices;
    daxa_BufferPtr(daxa_f32vec3) vertex_positions;
    daxa_BufferPtr(daxa_f32vec3) vertex_normals;
    daxa_BufferPtr(daxa_f32vec2) vertex_uvs;
};

DAXA_DECL_BUFFER_PTR_ALIGN(Mesh, 8)

struct MeshletData {
    daxa_u32 mesh_index;
    daxa_u32 mesh_group_index;
    daxa_u32 meshlet_index;
};
DAXA_DECL_BUFFER_PTR(MeshletData)

struct MeshletsData {
    daxa_u32 count;
    MeshletData meshlets[MAX_SURVIVING_MESHLETS];
};
DAXA_DECL_BUFFER_PTR(MeshletsData)

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
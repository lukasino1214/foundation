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
    f32 vertical_fov;
};
DAXA_DECL_BUFFER_PTR(CameraInfo)

struct CullData {
    u32vec2 hiz_size;          
    f32vec2 hiz_size_rcp;          
    u32vec2 physical_hiz_size;  
    f32vec2 physical_hiz_size_rcp;
};

struct GlobalSamplers {
    daxa_SamplerId linear_clamp;
    daxa_SamplerId linear_repeat;
    daxa_SamplerId nearest_repeat;
    daxa_SamplerId nearest_clamp;
    daxa_SamplerId linear_repeat_ani;
    daxa_SamplerId nearest_repeat_ani;
};

struct ShaderGlobals {
    CameraInfo main_camera;
    CameraInfo observer_camera;
    b32 render_as_observer;
    u32vec2 render_target_size;
    f32vec2 render_target_size_inv;
    f32vec3 sun_direction;
    CullData cull_data;
    GlobalSamplers samplers;
    daxa_BufferPtr(ShaderDebugBufferHead) debug;
};
DAXA_DECL_BUFFER_PTR(ShaderGlobals)

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

struct AABB {
    f32vec3 center;
    f32vec3 extent;
};
DAXA_DECL_BUFFER_PTR(AABB)

struct MeshGeometryData {
    daxa_BufferPtr(f32vec3) positions;
    daxa_BufferPtr(f32vec3) normals;
    daxa_BufferPtr(f32vec2) uvs;
    daxa_BufferPtr(u32) indices;

    daxa_BufferPtr(Meshlet) meshlets;
    daxa_BufferPtr(u32) indirect_vertices;
    daxa_BufferPtr(u32) micro_indices;
    daxa_BufferPtr(BoundingSphere) bounding_spheres;
    daxa_BufferPtr(AABB) aabbs;

    AABB mesh_aabb;
    u32 material_index;
    u32 vertex_count;
    u32 index_count;
    u32 meshlet_count;
};
DAXA_DECL_BUFFER_PTR(MeshGeometryData)

struct MeshGroupInstance {
    u32 mesh_indices_offset;
    u32 count;
};
DAXA_DECL_BUFFER_PTR_ALIGN(MeshGroupInstance, 8)

struct MeshInstance {
    daxa_BufferId mesh_buffer;

    daxa_BufferPtr(Meshlet) meshlets;
    daxa_BufferPtr(u32) indirect_vertices;
    daxa_BufferPtr(u32) micro_indices;
    daxa_BufferPtr(BoundingSphere) bounding_spheres;
    daxa_BufferPtr(AABB) aabbs;

    daxa_BufferPtr(f32vec3) positions;
    daxa_BufferPtr(f32vec3) normals;
    daxa_BufferPtr(f32vec2) uvs;

    AABB mesh_aabb;
    u32 mesh_group_index;
    u32 material_index;
    u32 meshlet_count;
};
DAXA_DECL_BUFFER_PTR_ALIGN(MeshInstance, 8)

struct MeshletInstance {
    u32 mesh_instance_index;
    u32 meshlet_index;
};
DAXA_DECL_BUFFER_PTR_ALIGN(MeshletInstance, 8)

struct CulledMeshInstancesData {
    u32 count;
    daxa_BufferPtr(u32) mesh_instances;
};
DAXA_DECL_BUFFER_PTR(CulledMeshInstancesData)

struct CulledMeshletInstancesRegion {
    u32 count;
    u32 offset;
    daxa_BufferPtr(MeshletInstance) meshlet_instances;
};
DAXA_DECL_BUFFER_PTR(CulledMeshletInstancesRegion)

struct PrefixSumWorkExpansion {
    u64 merged_expansion_count_thread_count;
    u32 expansions_max;
    u32 workgroup_count;
    daxa_BufferPtr(u32) expansions_inclusive_prefix_sum;
    daxa_BufferPtr(u32) expansions_src_work_item;
    daxa_BufferPtr(u32) expansions_expansion_factor;
};
DAXA_DECL_BUFFER_PTR(PrefixSumWorkExpansion)

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
    b32 normal_image_compressed;
};
DAXA_DECL_BUFFER_PTR(Material)

struct PointLight {
    f32vec3 position;
    f32vec3 color;
    f32 intensity;
    f32 range;
};
DAXA_DECL_BUFFER_PTR(PointLight)

struct SpotLight {
    f32vec3 position;
    f32vec3 direction;
    f32vec3 color;
    f32 intensity;
    f32 range;
    f32 inner_cone_angle;
    f32 outer_cone_angle;
};
DAXA_DECL_BUFFER_PTR(SpotLight)

struct PointLightsData {
    u32 count;
    daxa_BufferPtr(PointLight) point_lights;
};
DAXA_DECL_BUFFER_PTR(PointLightsData)

struct SpotLightsData {
    u32 count;
    daxa_BufferPtr(SpotLight) spot_lights;
};
DAXA_DECL_BUFFER_PTR(SpotLightsData)

#if defined(__cplusplus)
#define SHARED_FUNCTION inline
#define SHARED_FUNCTION_INOUT(X) X&
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
        f32vec4(0.0f, 0.0f, 0.0f, 1.0f));
#else
    f32mat4x4 ret = f32mat4x4(
        f32vec4(mat[0], 0.0),
        f32vec4(mat[1], 0.0),
        f32vec4(mat[2], 0.0),
        f32vec4(mat[3], 1.0));
#endif
    return ret;
}

#if DAXA_LANGUAGE == DAXA_LANGUAGE_SLANG
extension ShaderGlobals {
    void debug_draw(ShaderDebugAABBDraw draw) {
        u32 index;
        InterlockedAdd(debug->aabb_draw_indirect_info.instance_count, 1, index);
        debug->aabb_draws[index] = draw;
    }

    void debug_draw(ShaderDebugCircleDraw draw) {
        u32 index;
        InterlockedAdd(debug->circle_draw_indirect_info.instance_count, 1, index);
        debug->circle_draws[index] = draw;
    }

    void debug_draw(ShaderDebugLineDraw draw) {
        u32 index;
        InterlockedAdd(debug->line_draw_indirect_info.instance_count, 1, index);
        debug->line_draws[index] = draw;
    }
};

SHARED_FUNCTION u32 get_micro_index(daxa_BufferPtr(u32) micro_indices, u32 index) {
    u32 packed = micro_indices[index / 4];
    return (packed >> ((index % 4) * 8)) & 0xFF;
}

SHARED_FUNCTION f32vec2 make_gather_uv(f32vec2 inv_src_size, u32vec2 top_left_index) {
    return (f32vec2(top_left_index) + 1.0f) * inv_src_size;
}

f32vec4 shuffle_arr2(f32vec4 local_values[2], u32 load_index) {
    f32vec4 load_0 = WaveReadLaneAt(local_values[0], (load_index % 32));
    f32vec4 load_1 = WaveReadLaneAt(local_values[1], (u32(max(0u, i32(load_index) - 32))));
    return load_index > 31 ? load_1 : load_0;
}

#endif

#define MESH_SHADER_WORKGROUP_X 32
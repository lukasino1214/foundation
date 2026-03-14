#pragma once
#include <daxa/daxa.inl>
#include "glue.inl"

#define INVALID_ID (~(0u))
#define WARP_SIZE 32
#define WARP_SIZE_MULTIPLE_MASK (WARP_SIZE - 1u)

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

struct PostprocessSettings {
    f32 min_luminance_log2;
    f32 max_luminance_log2;
    f32 luminance_adaption_tau;
    f32 exposure_bias;
    f32 calibration;
    f32 sensor_sensitivity;
    f32 luminance_log2_range;
    f32 inv_luminance_log2_range;
    f32 auto_exposure_histogram_clip_lo;
    f32 auto_exposure_histogram_clip_hi;
};
DAXA_DECL_BUFFER_PTR(PostprocessSettings)

#define PROFILE_LAYER_COUNT 2
// An atmosphere layer density which can be calculated as:
//   density = exp_term * exp(exp_scale * h) + linear_term * h + constant_term,
struct DensityProfileLayer {
    f32 layer_width;
    f32 exp_term;
    f32 exp_scale;
    f32 lin_term;
    f32 const_term;
};
DAXA_DECL_BUFFER_PTR(DensityProfileLayer)

struct SkySettings {
    u32vec2 transmittance_dimensions;
    u32 transmittance_step_count;
    u32vec2 multiscattering_dimensions;
    u32 multiscattering_step_count;
    u32vec2 sky_dimensions;
    u32 sky_step_count;

    // =============== Atmosphere =====================
    f32 sun_brightness;
    f32vec3 sun_direction;

    f32 atmosphere_bottom;
    f32 atmosphere_top;

    f32vec3 mie_scattering;
    f32vec3 mie_extinction;
    f32 mie_scale_height;
    f32 mie_phase_function_g;
    DensityProfileLayer mie_density[PROFILE_LAYER_COUNT];
    daxa_BufferPtr(DensityProfileLayer) mie_density_ptr; 

    f32vec3 rayleigh_scattering;
    f32 rayleigh_scale_height;
    DensityProfileLayer rayleigh_density[PROFILE_LAYER_COUNT];
    daxa_BufferPtr(DensityProfileLayer) rayleigh_density_ptr; 

    f32vec3 absorption_extinction;
    DensityProfileLayer absorption_density[PROFILE_LAYER_COUNT];
    daxa_BufferPtr(DensityProfileLayer) absorption_density_ptr;
};
DAXA_DECL_BUFFER_PTR_ALIGN(SkySettings, 8)

struct ShaderGlobals {
    CameraInfo main_camera;
    CameraInfo view_camera;
    u32 frame_index;
    f32 delta_time;
    b32 render_as_observer;
    u32vec2 render_target_size;
    f32vec2 render_target_size_inv;
    CullData cull_data;
    GlobalSamplers samplers;
    PostprocessSettings postprocess_settings;
    SkySettings sky_settings;
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
    daxa_BufferPtr(u32) normals;
    daxa_BufferPtr(u32) uvs;
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
    daxa_BufferPtr(u32) normals;
    daxa_BufferPtr(u32) uvs;

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

#define M_PI 3.14159265358979323846264338327950288
#define M_TAU 6.28318530717958647692528676655900577
#define M_FRAC_PI_2 1.57079632679489661923132169163975144
#define M_FRAC_PI_3 1.04719755119659774615421446109316763
#define M_FRAC_PI_4 0.785398163397448309615660845819875721
#define M_FRAC_PI_6 0.52359877559829887307710723054658381
#define M_FRAC_PI_8 0.39269908169872415480783042290993786
#define M_FRAC_1_PI 0.318309886183790671537767526745028724
#define M_FRAC_2_PI 0.636619772367581343075535053490057448
#define M_FRAC_2_SQRT_PI 1.12837916709551257389615890312154517
#define M_SQRT_2 1.41421356237309504880168872420969808
#define M_CBRT_2 1.2599210498948731647672106072782283505
#define M_FRAC_1_SQRT_2 0.707106781186547524400844362104849039
#define M_E 2.71828182845904523536028747135266250
#define M_LOG2_E 1.44269504088896340735992468100189214
#define M_LOG2_10 3.32192809488736234787031942948939018
#define M_LOG10_E 0.434294481903251827651128918916605082
#define M_LOG10_2 0.301029995663981195213738894724493027
#define M_LN_2 0.693147180559945309417232121458176568
#define M_LN_10 2.30258509299404568401799145468436421
#define M_PHI 1.61803398874989484820
#define M_PLASTIC 1.32471795724474602596
#define FLT_MAX 3.402823466e+38f

#define FP16_MIN_POS 0.000000059604645
#define FP16_MAX 65504

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656
#define M_GOLDEN_ANGLE 2.39996323
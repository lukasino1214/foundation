#pragma once
#include "glue.inl"
#include "shared.inl"

// funny number which doesnt lay in -1 and 1 range to detect if ndc is behind the eye
static const f32 INVALID_NDC_AABB_Z = 69.0f;

struct NdcAABB {
    f32vec3 ndc_min;
    f32vec3 ndc_max;
};

NdcAABB calculate_ndc_aabb_from_worldspace(in f32mat4x4 model_matrix, in f32mat4x4 projection_view_matrix, in AABB aabb) {
    bool behind_near_plane = false;
    NdcAABB ret;

    const f32mat4x4 mvp = mul(projection_view_matrix, model_matrix);
    const f32vec3 model_corner_position = aabb.center + aabb.extent * f32vec3(-1.0f, -1.0f, -1.0f);
    const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position,1));
    const f32vec3 ndc_corner_position = clipspace_corner_position.xyz * rcp(clipspace_corner_position.w);
    ret.ndc_min = ndc_corner_position.xyz;
    ret.ndc_max = ndc_corner_position.xyz;
    behind_near_plane = behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
    
    for (u32 i = 1; i < 8; i++) {
        const f32vec3 corner = f32vec3(i & 0x1, i & 0x2, i & 0x4) * f32vec3(1, 0.5f, 0.25f);
        const f32vec3 model_corner_position = aabb.center + aabb.extent * corner;
        const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position, 1.0f));
        const f32vec3 ndc_corner_position = clipspace_corner_position.xyz * rcp(clipspace_corner_position.w);
        ret.ndc_min = min(ret.ndc_min, ndc_corner_position.xyz);
        ret.ndc_max = max(ret.ndc_max, ndc_corner_position.xyz);
        behind_near_plane = behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
    }

    ret.ndc_min.x = max(ret.ndc_min.x, -1.0f);
    ret.ndc_min.y = max(ret.ndc_min.y, -1.0f);
    ret.ndc_max.x = min(ret.ndc_max.x,  1.0f);
    ret.ndc_max.y = min(ret.ndc_max.y,  1.0f);
    if(behind_near_plane) { ret.ndc_min = ret.ndc_max = f32vec3(0.0f, 0.0f, INVALID_NDC_AABB_Z); }
    return ret;
}

AABB calculate_worldspace_aabb_from_modelspace(in f32mat4x4 model_matrix, in AABB aabb) {
    AABB worldspace_aabb;
    worldspace_aabb.center = mul(model_matrix, f32vec4(aabb.center, 1.0)).xyz;
    
    const f32vec3 right = model_matrix[0].xyz * aabb.extent.x;
    const f32vec3 up = model_matrix[1].xyz * aabb.extent.y;
    const f32vec3 forward = (-model_matrix[2].xyz) * aabb.extent.z;

    worldspace_aabb.extent = f32vec3(
        abs(dot(f32vec3(1.0, 0.0, 0.0), right)) +
        abs(dot(f32vec3(1.0, 0.0, 0.0), up)) +
        abs(dot(f32vec3(1.0, 0.0, 0.0), forward)),

        abs(dot(f32vec3(0.0, 1.0, 0.0), right)) +
        abs(dot(f32vec3(0.0, 1.0, 0.0), up)) +
        abs(dot(f32vec3(0.0, 1.0, 0.0), forward)),

        abs(dot(f32vec3(0.0, 0.0, 1.0), right)) +
        abs(dot(f32vec3(0.0, 0.0, 1.0), up)) +
        abs(dot(f32vec3(0.0, 0.0, 1.0), forward))
    );

    return worldspace_aabb;
}

bool is_worldspace_aabb_inside_plane(in f32vec3 center, in f32vec3 extent, in f32vec4 plane) {
    const f32vec3 normal = plane.xyz;
    const f32 radius = dot(extent, abs(normal));
    return (dot(normal, center) - plane.w) >= -radius;
}

bool is_worldspace_aabb_frusum_visible(in CameraInfo camera_info, in AABB aabb) {
    const u32 planes = 6;
    for (u32 i = 0; i < planes; ++i) {
        if (!is_worldspace_aabb_inside_plane(aabb.center, aabb.extent, camera_info.frustum_planes[i])) {
            return false;
        }
    }

    return true;
}

bool is_ndc_aabb_hiz_depth_visible(NdcAABB meshlet_ndc_aabb, u32vec2 hiz_res, daxa_ImageViewId hiz) {
    if(meshlet_ndc_aabb.ndc_max.z == INVALID_NDC_AABB_Z) { return true; }
    const f32vec2 f_hiz_resolution = hiz_res;
    const f32vec2 min_uv = (meshlet_ndc_aabb.ndc_min.xy + 1.0f) * 0.5f;
    const f32vec2 max_uv = (meshlet_ndc_aabb.ndc_max.xy + 1.0f) * 0.5f;
    const f32vec2 min_texel_i = floor(clamp(f_hiz_resolution * min_uv, f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f));
    const f32vec2 max_texel_i = floor(clamp(f_hiz_resolution * max_uv, f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f));
    const f32 pixel_range = max(max_texel_i.x - min_texel_i.x + 1.0f, max_texel_i.y - min_texel_i.y + 1.0f);
    const f32 mip = ceil(log2(max(2.0f, pixel_range))) - 1;

    i32 imip = i32(mip);
    const i32vec2 min_corner_texel = i32vec2(min_texel_i) >> imip;
    const i32vec2 max_corner_texel = i32vec2(max_texel_i) >> imip;
    if (any(greaterThan(max_corner_texel - min_corner_texel, i32vec2(1)))) {
        imip += 1;
    }
    const i32vec2 quad_corner_texel = i32vec2(min_texel_i) >> imip;
    const i32vec2 texel_bounds = max(i32vec2(0,0), (i32vec2(f_hiz_resolution) >> imip) - 1);

    const f32vec4 fetch = f32vec4(
        Texture2D<f32>::get(hiz).Load(i32vec3(clamp(quad_corner_texel + i32vec2(0,0), i32vec2(0,0), texel_bounds), imip)).x,
        Texture2D<f32>::get(hiz).Load(i32vec3(clamp(quad_corner_texel + i32vec2(0,1), i32vec2(0,0), texel_bounds), imip)).x,
        Texture2D<f32>::get(hiz).Load(i32vec3(clamp(quad_corner_texel + i32vec2(1,0), i32vec2(0,0), texel_bounds), imip)).x,
        Texture2D<f32>::get(hiz).Load(i32vec3(clamp(quad_corner_texel + i32vec2(1,1), i32vec2(0,0), texel_bounds), imip)).x
    );

    const f32 conservative_depth = min(min(fetch.x,fetch.y), min(fetch.z, fetch.w));
    const bool depth_cull = meshlet_ndc_aabb.ndc_max.z < conservative_depth;
    return !depth_cull;
}

bool is_triangle_invisible_micro_triangle(f32vec2 ndc_min, f32vec2 ndc_max, f32vec2 resolution) {
    f32 delta = 1.0 / 256.0f;
    f32vec2 sample_grid_min = (ndc_min * 0.5f + 0.5f) * resolution - 0.5f - delta;
    f32vec2 sample_grid_max = (ndc_max * 0.5f + 0.5f) * resolution - 0.5f + delta;

    bool prim_visible = !any(equal(floor(sample_grid_max), floor(sample_grid_min)));
    return !prim_visible && all(greaterThan(ndc_min, -f32vec2(0.99999,0.99999))) && all(lessThan(ndc_max, f32vec2(0.99999,0.99999)));
}

bool is_triangle_backfacing(f32vec4 clip_positions[3]) {
    return determinant(daxa_f32mat3x3(clip_positions[0].xyw, clip_positions[1].xyw, clip_positions[2].xyw)) > 0.00001f;
}
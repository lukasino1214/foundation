#pragma once
#include "shared.inl"

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

struct NdcAABB {
    f32vec3 ndc_min;
    f32vec3 ndc_max;
};

static const f32 INVALID_NDC_AABB_Z = 2.0f;

NdcAABB calculate_ndc_aabb(CameraInfo camera, f32mat4x4 model_matrix, AABB aabb) {
    bool initialized_min_max = false;
    bool max_behind_near_plane = false;
    NdcAABB ret;

    const float4x4 mvp = mul(camera.projection_view_matrix, model_matrix);
    const f32vec3 model_corner_position = aabb.center + aabb.extent * f32vec3(-1,-1,-1);
    const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position,1));
    const f32vec3 ndc_corner_position = clipspace_corner_position.xyz / clipspace_corner_position.w;
    ret.ndc_min = ndc_corner_position.xyz;
    ret.ndc_max = ndc_corner_position.xyz;
    max_behind_near_plane = max_behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
    for(i32 i = 1; i < 8; i++) {
        f32vec3 corner = f32vec3(i & 0x1, i & 0x2, i & 0x4) * f32vec3(2.0f, 1.0f, 0.5f) - 1.0f;
        const f32vec3 model_corner_position = aabb.center + aabb.extent * corner;
        const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position,1));
        const f32vec3 ndc_corner_position = clipspace_corner_position.xyz * rcp(clipspace_corner_position.w);
        ret.ndc_min = min(ret.ndc_min, ndc_corner_position.xyz);
        ret.ndc_max = max(ret.ndc_max, ndc_corner_position.xyz);
        max_behind_near_plane = max_behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
    }

    ret.ndc_min.x = max(ret.ndc_min.x, -1.0f);
    ret.ndc_min.y = max(ret.ndc_min.y, -1.0f);
    ret.ndc_max.x = min(ret.ndc_max.x, +1.0f);
    ret.ndc_max.y = min(ret.ndc_max.y, +1.0f);

    if(max_behind_near_plane) {
        ret.ndc_min = ret.ndc_max = f32vec3(0.0f,0.0f, INVALID_NDC_AABB_Z);
    }

    return ret;
}

bool is_ndc_aabb_hiz_depth_occluded(CullData data, CameraInfo camera, NdcAABB ndc_aabb, daxa_ImageViewId hiz) {
    if(ndc_aabb.ndc_max.z == INVALID_NDC_AABB_Z) { return false; }

    const i32 hiz_sample_width = 4;

    const f32vec2 f_hiz_resolution = data.hiz_size;
    const f32vec2 min_uv = (ndc_aabb.ndc_min.xy + 1.0f) * 0.5f;
    const f32vec2 max_uv = (ndc_aabb.ndc_max.xy + 1.0f) * 0.5f;

    const f32vec2 min_texel_i = clamp(f_hiz_resolution * min_uv, f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f);
    const f32vec2 max_texel_i = clamp(f_hiz_resolution * max_uv, f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f);

    const f32 pixel_width = max(max_texel_i.x - min_texel_i.x, max_texel_i.y - min_texel_i.y);
    const f32 mip = max(0.0f, ceil(log2(pixel_width)) - log2(f32(hiz_sample_width)));

    const i32 imip = i32(mip);
    const i32vec2 min_corner_texel = i32vec2(min_texel_i) >> imip;
    const i32vec2 max_corner_texel = i32vec2(max_texel_i) >> imip;
    const i32vec2 at_mip_pixel_width = max_corner_texel - min_corner_texel + i32vec2(1);
    const i32vec2 quad_corner_texel = i32vec2(min_texel_i) >> imip;
    const i32vec2 physical_texel_bounds = max(i32vec2(0,0), (data.physical_hiz_size >> imip) - 1);

    f32 conservative_depth = 1.0f;
    {
        for (i32 x = 0; x < (hiz_sample_width+1); x++) {
            for (i32 y = 0; y < hiz_sample_width+1; y++) {
                if (x >= at_mip_pixel_width.x || y >= at_mip_pixel_width.y) continue;
                f32 fetch = Texture2D<f32>::get(hiz).Load(i32vec3(clamp(quad_corner_texel + i32vec2(x,y), i32vec2(0,0), physical_texel_bounds), imip)).x;
                conservative_depth = min(conservative_depth, fetch);
            }
        }
    }
    const bool depth_cull = ndc_aabb.ndc_max.z < conservative_depth;

    return depth_cull;
}

AABB calculate_worldspace_aabb_from_modelspace(in f32mat4x4 transform_matrix, in AABB aabb) {
    const f32vec3 basis_x = mul(transform_matrix, f32vec4(aabb.extent.x, 0.0f, 0.0f, 0.0f)).xyz;
    const f32vec3 basis_y = mul(transform_matrix, f32vec4(0.0f, aabb.extent.y, 0.0f, 0.0f)).xyz;
    const f32vec3 basis_z = mul(transform_matrix, f32vec4(0.0f, 0.0f, aabb.extent.z, 0.0f)).xyz;
    
    AABB worldspace_aabb;
    worldspace_aabb.center = mul(transform_matrix, f32vec4(aabb.center, 1.0)).xyz;
    worldspace_aabb.extent = abs(basis_x) + abs(basis_y) + abs(basis_z);
    return worldspace_aabb;
}

bool is_triangle_invisible_micro_triangle(const f32vec2 ndc_min, const f32vec2 ndc_max, f32vec2 resolution) {
    const f32 delta = 1.0 / 256.0f;
    const f32vec2 sample_grid_min = (ndc_min * 0.5f + 0.5f) * resolution - 0.5f - delta;
    const f32vec2 sample_grid_max = (ndc_max * 0.5f + 0.5f) * resolution - 0.5f + delta;
    const bool primitive_visible = !any(equal(floor(sample_grid_max), floor(sample_grid_min)));
    return !primitive_visible && all(greaterThan(ndc_min, -f32vec2(0.99999,0.99999))) && all(lessThan(ndc_max, f32vec2(0.99999,0.99999)));
}

bool is_triangle_backfacing(f32vec4 triangle_clip_positions[3]) {
    return determinant(f32mat3x3(triangle_clip_positions[0].xyw, triangle_clip_positions[1].xyw, triangle_clip_positions[2].xyw)) >= 0.00001;
}
#include "daxa/daxa.inl"
#include "cull_lights.inl"
#include "../../../shader_library/cull_util.slang"

groupshared u32 point_light_count;
groupshared u32 spot_light_count;
groupshared u32 point_light_indices[256];
groupshared u32 spot_light_indices[256];

groupshared u32 shared_min_depth;
groupshared u32 shared_max_depth;

groupshared u32 depth_mask;

struct Sphere {
    f32vec3 origin;
    f32 radius;
}

bool sphere_inside_plane(const Sphere sphere, const Plane plane) {
    const f32 signed_distance = dot(sphere.origin, plane.normal) + plane.distance;
    return signed_distance < -sphere.radius;
}

bool frustum_cull_sphere_viewspace(const Plane planes[4], Sphere sphere, const f32 near, const f32 far) {
    bool result = (sphere.origin.z - sphere.radius > near) || (sphere.origin.z + sphere.radius < far);
    for (u32 i = 0; i < 4 && !result; i++) {
        result = result || sphere_inside_plane(sphere, planes[i]);
    }

    return result;
}

u32 construct_mask(const Sphere sphere, const f32 depth_range_min, const f32 depth_range_rcp) {
    const f32 f_min = sphere.origin.z + sphere.radius;
    const f32 f_max = sphere.origin.z - sphere.radius;

    const u32 mask_index_start = clamp(u32(floor((f_min - depth_range_min) * depth_range_rcp)), 0, 31);
    const u32 mask_index_end = clamp(u32(floor((f_max - depth_range_min) * depth_range_rcp)), 0, 31);
    
    u32 mask = ~0u;
    mask >>= 31 - (mask_index_end - mask_index_start);
    mask <<= mask_index_start;
    return mask;
}

[[vk::push_constant]] CullLightsPush push;
f32 screen_to_view(f32 depth) {
    const f32mat4x4 projection = push.uses.u_globals.main_camera.projection_matrix;
    return -projection[2][3] * rcp(projection[2][2] + depth);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cull_lights(u32vec2 pixel : SV_DispatchThreadID, u32vec2 tile_index : SV_GroupID, u32 local_index : SV_GroupIndex) {
    const u32vec2 resolution = push.uses.u_globals.render_target_size;
    const u32vec2 tile_frustums = u32vec2(round_up_div(resolution.x, 16), round_up_div(resolution.y, 16));

    if(local_index == 0) {
        point_light_count = 0;
        spot_light_count = 0;
        
        shared_min_depth = 0x7f7fffff;
        shared_max_depth = 0;

        depth_mask = 0;
    }

    GroupMemoryBarrierWithGroupSync();
    
    f32 depth = 0.0f;
    if(all(lessThan(pixel.xy, deref(push.uses.u_globals).render_target_size))) {
        depth = push.uses.u_depth_image.get()[pixel.xy];
        if(depth != 0.0f) {
            u32 u_depth = asuint(-screen_to_view(depth));
            InterlockedMin(shared_min_depth, u_depth);
            InterlockedMax(shared_max_depth, u_depth);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    const f32 near_depth_vs = screen_to_view(1.0f);
    const f32 far_depth_vs = screen_to_view(0.0f);

    const f32 min_depth_vs = -asfloat(shared_min_depth);
    const f32 max_depth_vs = -asfloat(shared_max_depth);

    const f32 depth_range_rcp = 32.0f / (max_depth_vs - min_depth_vs);
    if(all(lessThan(pixel.xy, deref(push.uses.u_globals).render_target_size))) {
        const u32 depth_mask_index = u32(max(0, min(32, floor((screen_to_view(depth) - min_depth_vs) * depth_range_rcp))));
        InterlockedOr(depth_mask, 1 << depth_mask_index);
    }

    const TileFrustum frustum = push.uses.u_tile_frustums[tile_index.y * tile_frustums.x + tile_index.x];
    for(u32 i = local_index; i < push.uses.u_point_lights.count; i += 16 * 16) {
        const PointLight light = push.uses.u_point_lights.point_lights[i];
        const f32vec3 light_position = mul(push.uses.u_globals.main_camera.view_matrix, f32vec4(light.position, 1.0f)).xyz;
        Sphere sphere = Sphere(light_position, light.range);

        if(!frustum_cull_sphere_viewspace(frustum.planes, sphere, min_depth_vs, max_depth_vs) && bool(depth_mask & construct_mask(sphere, min_depth_vs, depth_range_rcp))) {
            u32 index;
            InterlockedAdd(point_light_count, 1, index);
            point_light_indices[index] = i;
        }
    }

    for(u32 i = local_index; i < push.uses.u_spot_lights.count; i += 16 * 16) {
        const SpotLight light = push.uses.u_spot_lights.spot_lights[i];
        const f32vec3 light_position = mul(push.uses.u_globals.main_camera.view_matrix, f32vec4(light.position, 1.0f)).xyz;
        const f32vec3 light_direction = normalize(mul(f32mat3x3(push.uses.u_globals.main_camera.view_matrix), light.direction));


        const f32 range = light.range;
        const f32 radius = range * 0.5f / (light.outer_cone_angle * light.outer_cone_angle);
        const Sphere sphere = Sphere(light_position + light_direction * radius, radius);

        if(!frustum_cull_sphere_viewspace(frustum.planes, sphere, min_depth_vs, max_depth_vs) && bool(depth_mask & construct_mask(sphere, min_depth_vs, depth_range_rcp))) {
            u32 index;
            InterlockedAdd(spot_light_count, 1, index);
            spot_light_indices[index] = i;
        }
    }


    GroupMemoryBarrierWithGroupSync();

    // InterlockedAdd(push.uses.u_overdraw_image.get()[u32vec2(pixel.xy)], point_light_count);
    // InterlockedAdd(push.uses.u_overdraw_image.get()[u32vec2(pixel.xy)], spot_light_count);

    TileData* tile_data = push.uses.u_tile_data + (tile_index.y * tile_frustums.x + tile_index.x);
    if(local_index == 0) {
        tile_data.point_light_count = point_light_count;
        tile_data.spot_light_count = spot_light_count;
    }

    for(u32 i = local_index; i < point_light_count; i += 16 * 16) {
        tile_data.point_light_indices[i] = point_light_indices[i];
    }

    for(u32 i = local_index; i < spot_light_count; i += 16 * 16) {
        tile_data.spot_light_indices[i] = spot_light_indices[i];
    }
}
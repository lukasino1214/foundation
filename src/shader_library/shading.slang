#pragma once
#include "glue.inl"
#include "shared.inl"

f32vec3 pixel_index_to_view_space(CameraInfo camera, f32vec2 render_target_size_inv, f32vec2 pixel_index, f32 depth) {
    const f32vec2 uv_xy = (pixel_index + 0.5f) * render_target_size_inv;
    const f32vec4 clip_space_position = f32vec4(uv_xy * 2.0 - 1.0, depth, 1.0);
    const f32vec4 view_space_position = mul(camera.inverse_projection_matrix, clip_space_position);
    return view_space_position.xyz / view_space_position.w;
}

f32vec3 pixel_index_to_world_space(CameraInfo camera, f32vec2 render_target_size_inv, f32vec2 pixel_index, f32 depth) {
    const f32vec2 uv_xy = (pixel_index + 0.5f) * render_target_size_inv;
    const f32vec4 clip_space_position = f32vec4(uv_xy * 2.0 - 1.0, depth, 1.0);
    const f32vec4 view_space_position = mul(camera.inverse_projection_matrix, clip_space_position);
    const f32vec4 world_space_position = mul(camera.inverse_view_matrix, view_space_position / view_space_position.w);
    return world_space_position.xyz;
}

f32 calculate_point_light_attenuation(f32 to_light_dist, f32 cutoff) {
    f32 win = (to_light_dist / cutoff);
    win = win * win * win * win;
    win = max(0.0, 1.0 - win);
    win = win * win;
    const f32 attenuation = win / (to_light_dist * to_light_dist + 0.1);
    return attenuation;
}

f32 calculate_spot_light_attenuation(float3 to_light_dir, f32 to_light_dist, SpotLight light) {
    f32 win = (to_light_dist / light.range);
    win = win * win * win * win;
    win = max(0.0, 1.0 - win);
    win = win * win;
    const f32 distance_attenuation = win / (to_light_dist * to_light_dist + 0.1);

    const f32 cos_outer = cos(light.outer_cone_angle);
    const f32 spot_scale = 1.0 / max(cos(light.inner_cone_angle) - cos_outer, 1e-4);
    const f32 spot_offset = -cos_outer * spot_scale;
    const f32 cd = dot(-to_light_dir, light.direction);

    f32 angle_attenuation = clamp(cd * spot_scale + spot_offset, 0.0, 1.0);
    angle_attenuation = angle_attenuation * angle_attenuation;
    const f32 attenuation = distance_attenuation * angle_attenuation;
    return attenuation;
}

f32vec3 shade(f32vec3 diffuse, f32vec3 world_position, f32vec3 normal, SunLight* sun, PointLightsData* point_lights, SpotLightsData* spot_lights) {
    const f32 ambient = 0.1f;
    f32vec3 color = diffuse;

    {
        const SunLight* light = sun;
        const f32 diffuse = max(dot(normal, -light.direction), ambient);

        color *= diffuse * light.color * light.intensity;
    }

    for(u32 i = 0; i < point_lights.count; i++) {
        const PointLight light = point_lights.point_lights[i];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_point_light_attenuation(to_light_dist, light.range);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    for(u32 i = 0; i < spot_lights.count; i++) {
        const SpotLight light = spot_lights.spot_lights[i];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_spot_light_attenuation(position_to_light, to_light_dist, light);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    return color;
}

f32vec3 shade_opaque_tiled(f32vec3 diffuse, f32vec3 world_position, f32vec3 normal, TileData* tile_data, SunLight* sun, PointLightsData* point_lights, SpotLightsData* spot_lights) {
    const f32 ambient = 0.1f;
    f32vec3 color = diffuse;

    {
        const SunLight* light = sun;
        const f32 diffuse = max(dot(normal, -light.direction), ambient);

        color *= diffuse * light.color * light.intensity;
    }

    for(u32 i = 0; i < tile_data.opaque_point_light_count; i++) {
        const u32 index = tile_data.opaque_point_light_indices[i];
        const PointLight light = point_lights.point_lights[index];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_point_light_attenuation(to_light_dist, light.range);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    for(u32 i = 0; i < tile_data.opaque_spot_light_count; i++) {
        const u32 index = tile_data.opaque_spot_light_indices[i];
        const SpotLight light = spot_lights.spot_lights[index];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_spot_light_attenuation(position_to_light, to_light_dist, light);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    return color;
}

f32vec3 shade_transparent_tiled(f32vec3 diffuse, f32vec3 world_position, f32vec3 normal, TileData* tile_data, SunLight* sun, PointLightsData* point_lights, SpotLightsData* spot_lights) {
    const f32 ambient = 0.1f;
    f32vec3 color = diffuse;

    {
        const SunLight* light = sun;
        const f32 diffuse = max(dot(normal, -light.direction), ambient);

        color *= diffuse * light.color * light.intensity;
    }

    for(u32 i = 0; i < tile_data.transparent_point_light_count; i++) {
        const u32 index = tile_data.transparent_point_light_indices[i];
        const PointLight light = point_lights.point_lights[index];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_point_light_attenuation(to_light_dist, light.range);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    for(u32 i = 0; i < tile_data.transparent_spot_light_count; i++) {
        const u32 index = tile_data.transparent_spot_light_indices[i];
        const SpotLight light = spot_lights.spot_lights[index];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_spot_light_attenuation(position_to_light, to_light_dist, light);

        color += diffuse * light.color * light.intensity * attenuation;
    }

    return color;
}
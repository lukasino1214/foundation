#include "daxa/daxa.inl"
#include "calculate_frustums.inl"

Plane compute_plane(const f32vec3 p0, const f32vec3 p1, const f32vec3 p2) {
    Plane plane;

    f32vec3 v1 = p1 - p0;
    f32vec3 v2 = p2 - p0;

    plane.normal = normalize(cross(v1, v2));
    plane.distance = -dot(p0, plane.normal);
    return plane;
}

[[vk::push_constant]] CalculateFrustumsPush push;

f32vec3 screen_to_view(f32vec4 uv) {
    const f32vec4 p = mul(push.uses.u_globals.main_camera.inverse_projection_matrix, f32vec4(uv.xy * 2.0f - 1.0f, uv.z, uv.w));
    return p.xyz / p.w;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void calculate_frustums(u32vec2 tile_index : SV_DispatchThreadID) {
    const u32vec2 resolution = push.uses.u_globals.render_target_size;
    const u32vec2 tile_frustums = u32vec2(round_up_div(resolution.x, 16), round_up_div(resolution.y, 16));
    if(any(tile_index >= tile_frustums)) { return; }

    const f32vec3 camera_position = f32vec3(0.0f);
    const f32 far_plane = 0.0f;

    const f32vec2 tile_corner_points[4] = {
        f32vec2(tile_index.x, tile_index.y),
        f32vec2(tile_index.x + 1, tile_index.y),
        f32vec2(tile_index.x, tile_index.y + 1),
        f32vec2(tile_index.x + 1, tile_index.y + 1),
    };

    f32vec3 points[4];
    for(u32 i = 0; i < 4; i++) {
        points[i] = screen_to_view(f32vec4(tile_corner_points[i] * PIXELS_PER_FRUSTUM * push.uses.u_globals.render_target_size_inv, -1.0f, 1.0f));
    }

    TileFrustum frustum;
    frustum.planes[0] = compute_plane(camera_position, points[2], points[0]);
    frustum.planes[1] = compute_plane(camera_position, points[1], points[3]);
    frustum.planes[2] = compute_plane(camera_position, points[0], points[1]);
    frustum.planes[3] = compute_plane(camera_position, points[3], points[2]);

    const u32 index = tile_index.y * tile_frustums.x + tile_index.x;
    push.uses.u_tile_frustums[index] = frustum;
}
#define DAXA_RAY_TRACING 1
#include "daxa/daxa.inl"
#include "ray_tracing.inl"
#include "daxa/utils/task_graph.inl"

[[vk::push_constant]] RayTracePush push;

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

struct RayPayload {
    f32vec3 color;
};

[shader("raygeneration")]
void ray_gen() {
    const i32vec2 index = DispatchRaysIndex().xy;
    const f32vec2 screen_uv = index * push.uses.u_globals->render_target_size_inv;
    const f32vec2 ndc = screen_uv * 2.0f - 1.0f;

    f32vec4 target = mul(push.uses.u_globals->main_camera.inverse_projection_matrix, f32vec4(ndc.x, ndc.y, 1, 1));
    f32vec4 direction = mul(push.uses.u_globals->main_camera.inverse_view_matrix, f32vec4(normalize(target.xyz), 0));

    RayDesc ray = {};
    ray.Origin = push.uses.u_globals->main_camera.position;
    ray.Direction = direction.xyz;
    ray.TMax = 10000.0f;
    ray.TMin = 0.00001f;

    RayPayload payload = {};
    TraceRay(push.uses.tlas.get(), RAY_FLAG_NONE, ~0, 0, 0, 0, ray, payload);

    RWTexture2D<f32vec4> image = RWTexture2D<f32vec4>::get(push.uses.u_image);
    image[index.xy] = f32vec4(payload.color, 1.0f);
}

[shader("anyhit")]
void any_hit() {
    // IgnoreHit();
}

[shader("closesthit")]
void closest_hit(inout RayPayload payload) {
    u32 primitive_id = PrimitiveIndex();
    payload.color = hsv_to_rgb(f32vec3(f32(primitive_id) / 128 * M_GOLDEN_CONJ, 0.875, 0.85));
}

[shader("miss")]
void miss(inout RayPayload payload) {
    payload.color = f32vec3(0.0f, 0.0f, 0.0f);
}
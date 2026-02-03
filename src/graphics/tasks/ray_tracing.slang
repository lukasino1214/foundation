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
#define M_PI 3.14159265358979323846

struct RayPayload {
    f32vec3 color;
    f32 cone_width;
    f32 cone_spread;
    f32 hit_distance;
};

[shader("raygeneration")]
void ray_gen() {
    const i32vec2 index = DispatchRaysIndex().xy;
    const f32vec2 screen_uv = (index + 0.5f) * push.uses.u_globals->render_target_size_inv;
    const f32vec2 ndc = screen_uv * 2.0f - 1.0f;

    f32vec4 target = mul(push.uses.u_globals->main_camera.inverse_projection_matrix, f32vec4(ndc.x, ndc.y, 1, 1));
    f32vec4 direction = mul(push.uses.u_globals->main_camera.inverse_view_matrix, f32vec4(normalize(target.xyz), 0));

    RayDesc ray = {};
    ray.Origin = push.uses.u_globals->main_camera.position;
    ray.Direction = direction.xyz;
    ray.TMax = 10000.0f;
    ray.TMin = 0.00001f;

    RayPayload payload = {};
    payload.cone_width = 0.0f;
    payload.cone_spread = 2.0 * atan(2.0 * tan(push.uses.u_globals->main_camera.vertical_fov * 0.5) / f32(push.uses.u_globals->render_target_size.y));
    payload.hit_distance = 0.0f;

    TraceRay(daxa::acceleration_structures[push.uses.u_tlas.index()], RAY_FLAG_NONE, ~0, 0, 0, 0, ray, payload);

    RWTexture2D<f32vec4> image = RWTexture2D<f32vec4>::get(push.uses.u_image);
    image[index.xy] = f32vec4(payload.color, 1.0f);
}

f32vec2 interpolate_vec2(f32vec3 barycentrics, f32vec2 v0, f32vec2 v1, f32vec2 v2) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

f32vec3 interpolate_vec3(f32vec3 barycentrics, f32vec3 v0, f32vec3 v1, f32vec3 v2) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

void compute_derivatives_using_ray_cones(
    in f32vec3 hit_point,
    in f32vec3 normal,
    in f32vec3 ray_dir,
    in f32 cone_width_at_hit,
    in f32vec3 positions[3],        
    in f32vec2 texcoords[3],        
    in f32vec2 interpolated_uv,
    out f32vec2 tex_gradient_1,
    out f32vec2 tex_gradient_2
) {
    f32 r = cone_width_at_hit * 0.5;
    f32vec3 h1 = ray_dir - dot(normal, ray_dir) * normal;

    f32vec3 p1 = h1 - dot(ray_dir, h1) * ray_dir;
    f32 len_p1 = length(p1);

    f32 scale1 = r / max(0.0001, len_p1);
    f32vec3 a1 = h1 * scale1;

    f32vec3 h2 = cross(normal, h1);

    f32vec3 p2 = h2 - dot(ray_dir, h2) * ray_dir;
    f32 len_p2 = length(p2);
    f32 scale2 = r / max(0.0001, len_p2);
    f32vec3 a2 = h2 * scale2;

    f32vec3 e1 = positions[1] - positions[0];
    f32vec3 e2 = positions[2] - positions[0];
    f32vec3 f_cross_e1 = cross(normal, e1);
    f32vec3 f_cross_e2 = cross(normal, e2);

    f32 one_over_tri_area = 1.0 / dot(normal, cross(e1, e2));
    f32vec3 delta = hit_point - positions[0];

    f32vec3 aP = delta + a1;
    f32 u1 = dot(normal, cross(aP, e2)) * one_over_tri_area;
    f32 v1 = dot(normal, cross(e1, aP)) * one_over_tri_area;

    f32vec2 uv_at_a1 = (1.0 - u1 - v1) * texcoords[0] + u1 * texcoords[1] + v1 * texcoords[2];
    tex_gradient_1 = uv_at_a1 - interpolated_uv;

    aP = delta + a2;
    f32 u2 = dot(normal, cross(aP, e2)) * one_over_tri_area;
    f32 v2 = dot(normal, cross(e1, aP)) * one_over_tri_area;
    
    f32vec2 uv_at_a2 = (1.0 - u2 - v2) * texcoords[0] + u2 * texcoords[1] + v2 * texcoords[2];
    tex_gradient_2 = uv_at_a2 - interpolated_uv;
}

[shader("anyhit")]
void any_hit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const MeshGeometryData mesh = push.uses.u_meshes[InstanceID()];
    const f32vec3 barycentrics = f32vec3(1.0f - (attr.barycentrics.x + attr.barycentrics.y), attr.barycentrics.x, attr.barycentrics.y);
    
    const f32 t = RayTCurrent();
    const f32vec3 ray_dir = normalize(WorldRayDirection());
    
    const f32 current_cone_width = payload.cone_width + payload.cone_spread * (t - payload.hit_distance);
    
    const u32 primitive_index = PrimitiveIndex();
    const u32vec3 vertex_indices = u32vec3(
        mesh.indices[primitive_index * 3 + 0],
        mesh.indices[primitive_index * 3 + 1],
        mesh.indices[primitive_index * 3 + 2]
    );

    const f32vec3 p0 = mesh.positions[vertex_indices.x];
    const f32vec3 p1 = mesh.positions[vertex_indices.y];
    const f32vec3 p2 = mesh.positions[vertex_indices.z];
    const f32vec2 uv0 = mesh.uvs[vertex_indices.x];
    const f32vec2 uv1 = mesh.uvs[vertex_indices.y];
    const f32vec2 uv2 = mesh.uvs[vertex_indices.z];

    const f32vec2 uv = interpolate_vec2(barycentrics, uv0, uv1, uv2);
    const f32vec3 hit_point = interpolate_vec3(barycentrics, p0, p1, p2);
    const f32vec3 normal = normalize(cross(p1 - p0, p2 - p0));

    f32vec3 positions[3] = {p0, p1, p2};
    f32vec2 texcoords[3] = {uv0, uv1, uv2};

    f32vec2 uv_ddx, uv_ddy;
    compute_derivatives_using_ray_cones(
        hit_point,
        normal,
        ray_dir,
        current_cone_width,
        positions,
        texcoords,
        uv,
        uv_ddx,
        uv_ddy
    );

    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(primitive_index) / 128 * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    if(mesh.material_index != INVALID_ID) {
        Material material = push.uses.u_materials[mesh.material_index];

        if(!material.albedo_image_id.is_empty()) {
            color = Texture2D<f32vec4>::get(material.albedo_image_id).SampleGrad(SamplerState::get(material.albedo_sampler_id), uv, uv_ddx, uv_ddy);
        } else {
            color = material.albedo_factor;
        }

        if(!material.emissive_image_id.is_empty()) {
            color.rgb += Texture2D<f32vec4>::get(material.emissive_image_id).SampleGrad(SamplerState::get(material.emissive_sampler_id), uv, uv_ddx, uv_ddy).rgb;
        }
    }

    if(color.a < 0.5) {
        IgnoreHit();
    }

    payload.color = color.rgb;
    payload.hit_distance = t;
    payload.cone_width = current_cone_width;
}

[shader("closesthit")]
void closest_hit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const MeshGeometryData mesh = push.uses.u_meshes[InstanceID()];
    const f32vec3 barycentrics = f32vec3(1.0f - (attr.barycentrics.x + attr.barycentrics.y), attr.barycentrics.x, attr.barycentrics.y);
    
    const f32 t = RayTCurrent();
    const f32vec3 ray_dir = normalize(WorldRayDirection());
    
    const f32 current_cone_width = payload.cone_width + payload.cone_spread * (t - payload.hit_distance);
    
    const u32 primitive_index = PrimitiveIndex();
    const u32vec3 vertex_indices = u32vec3(
        mesh.indices[primitive_index * 3 + 0],
        mesh.indices[primitive_index * 3 + 1],
        mesh.indices[primitive_index * 3 + 2]
    );

    const f32vec3 p0 = mesh.positions[vertex_indices.x];
    const f32vec3 p1 = mesh.positions[vertex_indices.y];
    const f32vec3 p2 = mesh.positions[vertex_indices.z];
    const f32vec2 uv0 = mesh.uvs[vertex_indices.x];
    const f32vec2 uv1 = mesh.uvs[vertex_indices.y];
    const f32vec2 uv2 = mesh.uvs[vertex_indices.z];

    const f32vec2 uv = interpolate_vec2(barycentrics, uv0, uv1, uv2);
    const f32vec3 hit_point = interpolate_vec3(barycentrics, p0, p1, p2);
    const f32vec3 normal = normalize(cross(p1 - p0, p2 - p0));

    f32vec3 positions[3] = {p0, p1, p2};
    f32vec2 texcoords[3] = {uv0, uv1, uv2};

    f32vec2 uv_ddx, uv_ddy;
    compute_derivatives_using_ray_cones(
        hit_point,
        normal,
        ray_dir,
        current_cone_width,
        positions,
        texcoords,
        uv,
        uv_ddx,
        uv_ddy
    );

    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(primitive_index) / 128 * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    if(mesh.material_index != INVALID_ID) {
        Material material = push.uses.u_materials[mesh.material_index];

        if(!material.albedo_image_id.is_empty()) {
            color = Texture2D<f32vec4>::get(material.albedo_image_id).SampleGrad(SamplerState::get(material.albedo_sampler_id), uv, uv_ddx, uv_ddy);
        } else {
            color = material.albedo_factor;
        }

        if(!material.emissive_image_id.is_empty()) {
            color.rgb += Texture2D<f32vec4>::get(material.emissive_image_id).SampleGrad(SamplerState::get(material.emissive_sampler_id), uv, uv_ddx, uv_ddy).rgb;
        }
    }

    payload.color = color.rgb;
    payload.hit_distance = t;
    payload.cone_width = current_cone_width;
}

[shader("miss")]
void miss(inout RayPayload payload) {
    payload.color = f32vec3(0.0f, 0.0f, 0.0f);
}
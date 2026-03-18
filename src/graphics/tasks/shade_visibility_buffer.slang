#define DAXA_RAY_TRACING 1
#include "daxa/daxa.inl"
#include "shade_visibility_buffer.inl"
#include "../../shader_library/cull_util.slang"
#include "../../shader_library/visibility_buffer.slang"
#include "../../shader_library/compression.inl"
#include "../../shader_library/sky_util.glsl"

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

f32 calculate_point_light_attenuation(f32 to_light_dist, f32 cutoff) {
    f32 win = (to_light_dist / cutoff);
    win = win * win * win * win;
    win = max(0.0, 1.0 - win);
    win = win * win;
    const f32 attenuation = win / (to_light_dist * to_light_dist + 0.1);
    return attenuation;
}

f32 calculate_spot_light_attenuation(f32vec3 to_light_dir, f32 to_light_dist, SpotLight light) {
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

bool trace_shadow_ray(f32vec3 origin, f32vec3 direction, f32 max_distance) {
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> q;
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.001f;
    ray.TMax = max_distance;
    
    q.TraceRayInline(daxa::acceleration_structures[push.uses.u_tlas.index()], RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    q.Proceed();
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

f32vec3 get_sun_direct_lighting(
    ShaderGlobals* globals, 
    daxa_ImageViewId sky_transmittance,
    daxa_ImageViewId sky,
    f32vec3 view_direction, 
    f32vec3 atmo_position)
{
    const float bottom_atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        f32vec3(0.0, 0.0, length(atmo_position)),
        y_up_to_z_up(view_direction),
        f32vec3(0.0),
        globals.sky_settings.atmosphere_bottom
    );
    
    bool view_ray_intersects_ground = bottom_atmosphere_intersection_distance >= 0.0;
    const f32vec3 direct_sun_illuminance = view_ray_intersects_ground ? 
        f32vec3(0.0) : 
        get_sun_illuminance(
            &globals.sky_settings,
            sky_transmittance,
            globals->samplers.linear_clamp,
            y_up_to_z_up(view_direction),
            length(atmo_position),
            dot(y_up_to_z_up(globals.sky_settings.sun_direction), normalize(atmo_position))
        );

    return direct_sun_illuminance;
}

f32vec3 shade(f32vec3 world_position, f32vec3 normal, f32vec3 sun_direction, PointLightsData* point_lights, SpotLightsData* spot_lights) {
    f32vec3 color = f32vec3(0);

    f32vec3 atmo_position = get_atmo_position(push.uses.u_globals);

    {
        const f32 n_dot_l = max(dot(normal, sun_direction), 0.0f);

        f32 shadow = 1.0f;
        if (n_dot_l > 0.0f) {
            bool in_shadow = trace_shadow_ray(
                world_position + normal * 0.001f, 
                sun_direction, 
                1000.0f
            );
            shadow = in_shadow ? 0.0f : 1.0f;
        }
        
        color += n_dot_l * shadow * get_sun_direct_lighting(push.uses.u_globals, push.uses.transmittance, push.uses.sky, push.uses.u_globals.sky_settings.sun_direction, atmo_position);
    }

    for(u32 i = 0; i < point_lights.count; i++) {
        const PointLight light = point_lights.point_lights[i];
        const f32vec3 to_light = light.position - world_position;
        const f32 to_light_dist = length(to_light);

        if(to_light_dist > light.range) { continue; }

        const f32vec3 to_light_dir = normalize(to_light);
        const f32 n_dot_l = max(dot(normal, to_light_dir), 0.0f);

        f32 shadow = 1.0f;
        if (n_dot_l > 0.0f) {
            bool in_shadow = trace_shadow_ray(
                world_position + normal * 0.001f, 
                to_light_dir, 
                to_light_dist
            );
            shadow = in_shadow ? 0.0f : 1.0f;
        }

        const f32 attenuation = calculate_point_light_attenuation(to_light_dist, light.range);
        color += light.color * light.intensity * attenuation * n_dot_l * shadow;
    }


    for(u32 i = 0; i < spot_lights.count; i++) {
        const SpotLight light = spot_lights.spot_lights[i];
        const f32vec3 to_light = light.position - world_position;
        const f32 to_light_dist = length(to_light);

        if(to_light_dist > light.range) { continue; }

        const f32vec3 to_light_dir = normalize(to_light);
        const f32 n_dot_l = max(dot(normal, to_light_dir), 0.0f);

        f32 shadow = 1.0f;
        if (n_dot_l > 0.0f) {
            bool in_shadow = trace_shadow_ray(
                world_position + normal * 0.001f, 
                to_light_dir, 
                to_light_dist
            );
            shadow = in_shadow ? 0.0f : 1.0f;
        }

        const f32 attenuation = calculate_spot_light_attenuation(to_light_dir, to_light_dist, light);
        color += light.color * light.intensity * attenuation * n_dot_l * shadow;
    }

    return color;
}

void compute_uv_tangent(f32vec3 positions[3], f32vec2 uvs[3], out f32vec3 tangent, out f32vec3 bitangent) {
    const f32vec3 d_p1 = positions[1].xyz - positions[0].xyz;
    const f32vec3 d_p2 = positions[2].xyz - positions[0].xyz;

    const f32vec2 d_uv1 = uvs[1] - uvs[0];
    const f32vec2 d_uv2 = uvs[2] - uvs[0];

    const f32 r = 1.0f / (d_uv1.x * d_uv2.y - d_uv1.y * d_uv2.x);
    tangent = normalize((d_p1 * d_uv2.y - d_p2 * d_uv1.y) * r);
    bitangent = normalize((d_p2 * d_uv1.x - d_p1 * d_uv2.x) * r);
}

f32vec3 calculate_normal_from_map(f32vec3 tangent_normal, f32vec3 geometric_normal, f32vec3 world_position, f32vec2 uv, f32vec3 tangent, f32vec3 bitangent) {
    const f32mat3x3 tbn = transpose(f32mat3x3(tangent, bitangent, geometric_normal));
    return normalize(mul(tbn, tangent_normal));
}

[[vk::push_constant]] ShadeVisibilityBufferPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void shade_visibility_buffer(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }

    u32 visibility_index = push.uses.u_visibility_image.get()[pixel.xy];
    if(visibility_index == INVALID_ID) {
        const f32vec2 ray_uv = f32vec2(pixel.xy + 0.5f) / f32vec2(push.uses.u_globals.render_target_size);
        const f32vec2 ray_ndc = ray_uv * 2.0 - 1.0;
        const f32vec4 ray_view_space = mul(push.uses.u_globals.view_camera.inverse_projection_matrix, f32vec4(ray_ndc, -1.0, 0.0));
        const f32vec4 ray_world_space = mul(push.uses.u_globals.view_camera.inverse_view_matrix, f32vec4(ray_view_space.xy, -1.0, 0.0));
        const f32vec3 view_direction = normalize(ray_world_space.xyz);
        
        const f32vec3 atmo_position = get_atmo_position(push.uses.u_globals);
        const f32vec3 atmosphere_direct_illuminnace = get_atmosphere_illuminance_along_ray(
            push.uses.u_globals->sky_settings,
            push.uses.transmittance,
            push.uses.sky,
            push.uses.u_globals->samplers.linear_clamp,
            view_direction,
            atmo_position
        );
        const f32vec3 sun_direct_illuminance = get_sun_direct_lighting(
            push.uses.u_globals, push.uses.transmittance, push.uses.sky,
            view_direction, atmo_position);
        const f32vec3 total_direct_illuminance = sun_direct_illuminance + atmosphere_direct_illuminnace;

        push.uses.u_render_image.get()[pixel.xy] = f32vec4(total_direct_illuminance, 1.0f);
        return;
    }

    u32 meshlet_data_index = 0;
    u32 triangle_index = 0;
    decode_visibility_index(visibility_index, meshlet_data_index, triangle_index);

    const MeshletInstance meshlet_instance = push.uses.u_culled_meshlet_instances[meshlet_data_index];
    const MeshInstance mesh_instance = push.uses.u_mesh_instances[meshlet_instance.mesh_instance_index];
    const Meshlet meshlet = mesh_instance.meshlets[meshlet_instance.meshlet_index];

    const u32[3] indices = u32[3](
        mesh_instance.indirect_vertices[meshlet.indirect_vertex_offset + get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 0)],
        mesh_instance.indirect_vertices[meshlet.indirect_vertex_offset + get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 1)],
        mesh_instance.indirect_vertices[meshlet.indirect_vertex_offset + get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 2)]
    );

    const f32vec3[3] positions = f32vec3[3](
        mesh_instance.positions[indices[0]],
        mesh_instance.positions[indices[1]],
        mesh_instance.positions[indices[2]]
    );

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh_instance.mesh_group_index]); 
    const f32vec4[3] world_positions = f32vec4[3](
        mul(transform_matrix, f32vec4(positions[0], 1.0)),
        mul(transform_matrix, f32vec4(positions[1], 1.0)),
        mul(transform_matrix, f32vec4(positions[2], 1.0))
    );

    const f32mat4x4 projection_view_matrix = push.uses.u_globals->view_camera.projection_view_matrix;

    const f32vec4[3] clip_position = f32vec4[3](
        mul(projection_view_matrix, world_positions[0]),
        mul(projection_view_matrix, world_positions[1]),
        mul(projection_view_matrix, world_positions[2])
    ); 
    const bool is_backfacing = is_triangle_backfacing(clip_position);

    const f32vec2 ndc_xy = ((pixel.xy + 0.5f) * push.uses.u_globals->render_target_size_inv) * 2.0f - 1.0f;
    const BarycentricDeriv derivatives = calc_bary_and_deriv(clip_position[0], clip_position[1], clip_position[2], ndc_xy, f32vec2(push.uses.u_globals->render_target_size));

    f32vec3 position = interpolate_vec3(
        derivatives.m_lambda,
        world_positions[0].xyz,
        world_positions[1].xyz,
        world_positions[2].xyz
    );

    const f32vec2[3] uvs = mesh_instance.uvs != 0
        ? f32vec2[3](
            decode_uv(mesh_instance.uvs[indices[0]]),
            decode_uv(mesh_instance.uvs[indices[1]]),
            decode_uv(mesh_instance.uvs[indices[2]])
        )
        : f32vec2[3](f32vec2(0.0f), f32vec2(0.0f), f32vec2(0.0f));

    f32vec3 tangent;
    f32vec3 bitagent;
    compute_uv_tangent(f32vec3[3](world_positions[0].xyz, world_positions[1].xyz, world_positions[2].xyz), uvs, tangent, bitagent);

    const UvGrad uv_grad = get_uv_grad(derivatives, uvs[0], uvs[1], uvs[2]);

    const f32vec3[3] vertex_normals = mesh_instance.normals != 0
        ? f32vec3[3](
            decode_normal(mesh_instance.normals[indices[0]]),
            decode_normal(mesh_instance.normals[indices[1]]),
            decode_normal(mesh_instance.normals[indices[2]])
        )
        : f32vec3[3](f32vec3(0.0f, 1.0f, 0.0f), f32vec3(0.0f, 1.0f, 0.0f), f32vec3(0.0f, 1.0f, 0.0f));

    const f32mat3x3 normal_matrix = f32mat3x3(
        cross(transform_matrix[1].xyz, transform_matrix[2].xyz), 
        cross(transform_matrix[2].xyz, transform_matrix[0].xyz), 
        cross(transform_matrix[0].xyz, transform_matrix[1].xyz)
    );

    const f32vec3[3] worldspace_vertex_normals = f32vec3[3](
        mul(normal_matrix, vertex_normals[0]),
        mul(normal_matrix, vertex_normals[1]),
        mul(normal_matrix, vertex_normals[2])
    );

    f32vec3 normal = normalize(interpolate_vec3(
        derivatives.m_lambda,
        worldspace_vertex_normals[0].xyz,
        worldspace_vertex_normals[1].xyz,
        worldspace_vertex_normals[2].xyz
    ));
    normal = is_backfacing ? -normal : normal;

    f32vec4 albedo = f32vec4(hsv_to_rgb(f32vec3(f32(triangle_index) / 128 * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    if(mesh_instance.material_index != INVALID_ID) {
        const Material material = push.uses.u_materials[mesh_instance.material_index];

        if(!material.albedo_image_id.is_empty()) {
            albedo = Texture2D<f32vec4>::get(material.albedo_image_id).SampleGrad(SamplerState::get(material.albedo_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy);
        } else {
            albedo = material.albedo_factor;
        }
        
        if(!material.normal_image_id.is_empty()) {
            f32vec3 normal_sample = f32vec3(0.0f);
            if(material.normal_image_compressed == 1) {
                const f32vec2 raw_normal =  Texture2D<f32vec4>::get(material.normal_image_id).SampleGrad(SamplerState::get(material.normal_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).xy * 2.0f - 1.0f;
                normal_sample = f32vec3(raw_normal, sqrt(clamp(1.0f - dot(raw_normal, raw_normal), 0.0, 1.0)));
            } else {
                normal_sample = Texture2D<f32vec4>::get(material.normal_image_id).SampleGrad(SamplerState::get(material.normal_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).rgb * 2.0f - 1.0f;
            }
            
            normal = calculate_normal_from_map(
                normal_sample,
                normalize(normal),
                position,
                uv_grad.uv,
                tangent,
                bitagent
            );
        }
    }

    f32vec3 direct_lighting = shade(
        position, 
        normalize(normal), 
        push.uses.u_globals.sky_settings.sun_direction, 
        push.uses.u_point_lights, 
        push.uses.u_spot_lights
    );

    f32 ambient_occlusion = push.uses.u_gtao_image.get()[pixel.xy];
    f32vec4 compressed_indirect_lighting = TextureCube<float4>::get(push.uses.sky_ibl).SampleLevel(SamplerState::get(push.uses.u_globals->samplers.linear_clamp), normal, 0);
    f32vec3 indirect_lighting = compressed_indirect_lighting.rgb * M_PI * 2 * compressed_indirect_lighting.a;

    push.uses.u_render_image.get()[pixel.xy] = f32vec4(albedo.rgb * M_FRAC_1_PI * (direct_lighting + indirect_lighting * ambient_occlusion), 1);
}

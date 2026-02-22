#define DAXA_RAY_TRACING 1
#include "daxa/daxa.inl"
#include "draw_mesh.inl"

[[vk::push_constant]] DrawMeshPush push;

struct MeshStageOutput {
    f32vec4 sv_position : SV_Position;
    f32vec3 position;
    f32vec3 normal;
    f32vec2 uv;
};

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void mesh_main(
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<MeshStageOutput, MAX_VERTICES_PER_MESHLET> out_vertices) {
    const u32 thread_index = local_thread_index.x;

    const MeshInstance mesh_instance_data = push.mesh_instance_data;
    if(mesh_instance_data.mesh_buffer.value == 0) { return; }
    const Meshlet meshlet = mesh_instance_data.meshlets[group_index.x];

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh_instance_data.mesh_group_index]);
    const f32mat3x3 normal_matrix = f32mat3x3(
        cross(transform_matrix[1].xyz, transform_matrix[2].xyz), 
        cross(transform_matrix[2].xyz, transform_matrix[0].xyz), 
        cross(transform_matrix[0].xyz, transform_matrix[1].xyz)
    );

    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

    if(thread_index < meshlet.vertex_count) {
        const u32 vertex_index = mesh_instance_data.indirect_vertices[meshlet.indirect_vertex_offset + thread_index];

        MeshStageOutput vertex;
        vertex.position = mul(transform_matrix, f32vec4(mesh_instance_data.positions[vertex_index], 1.0f)).xyz;
        vertex.sv_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, f32vec4(vertex.position, 1.0f));
        vertex.normal = normalize(mul(normal_matrix, mesh_instance_data.normals[vertex_index]));

        if(mesh_instance_data.uvs != 0) {
            vertex.uv = mesh_instance_data.uvs[vertex_index];
        } else {
            vertex.uv = f32vec2(0.0f, 0.0f);
        }

        out_vertices[thread_index] = vertex;
    }

    if(thread_index < meshlet.triangle_count) {
        u32vec3 indices = u32vec3(
            get_micro_index(mesh_instance_data.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0),
            get_micro_index(mesh_instance_data.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1),
            get_micro_index(mesh_instance_data.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)
        );

        out_indices[thread_index] = indices;
    }
}

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

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

f32vec3 shade(f32vec3 diffuse, f32vec3 world_position, f32vec3 normal, f32vec3 sun_direction, PointLightsData* point_lights, SpotLightsData* spot_lights) {
    const f32 ambient = 0.3f;
    f32vec3 color = diffuse * ambient;

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
        
        color += diffuse * n_dot_l * f32vec3(1.0f) * 1.0f * shadow;
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
        color += diffuse * light.color * light.intensity * attenuation * n_dot_l * shadow;
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
        color += diffuse * light.color * light.intensity * attenuation * n_dot_l * shadow;
    }

    return color;
}

f32vec3 calculate_normal_from_map(f32vec3 tangent_normal, f32vec3 geometric_normal, f32vec3 world_position, f32vec2 uv) {
    tangent_normal.y *= -1.0;
    
    f32vec3 p_dx = ddx(world_position);
    f32vec3 p_dy = ddy(world_position);
    f32vec2 uv_dx = ddx(uv);
    f32vec2 uv_dy = ddy(uv);

    f32vec3 t = normalize(uv_dy.y * p_dx - uv_dx.y * p_dy);
    f32vec3 b = normalize(uv_dy.x * p_dx - uv_dx.x * p_dy);

    t = normalize(t - geometric_normal * dot(geometric_normal, t));
    b = cross(geometric_normal, t);

    f32mat3x3 tbn = transpose(f32mat3x3(t, b, geometric_normal));
    f32vec3 world_normal = mul(tbn, tangent_normal);
    
    return normalize(world_normal);
}

[shader("fragment")]
f32vec4 fragment_main(u32 primitive_id : SV_PrimitiveID, bool is_front_facing : SV_IsFrontFace, in MeshStageOutput input) : SV_Target {
    const MeshInstance mesh_instance_data = push.mesh_instance_data;

    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(primitive_id) / 128 * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    f32vec3 normal = input.normal;
    if(mesh_instance_data.material_index != INVALID_ID) {
        const Material material = push.uses.u_materials[mesh_instance_data.material_index];

        if(!material.albedo_image_id.is_empty()) {
            color = Texture2D<f32vec4>::get(material.albedo_image_id).Sample(SamplerState::get(material.albedo_sampler_id), input.uv);
            f32 lod = Texture2D<f32vec4>::get(material.albedo_image_id).CalculateLevelOfDetail(SamplerState::get(material.albedo_sampler_id), input.uv);
            color.a *= 1.0f + max(0.0f, lod) * 0.25f;
            color.a = saturate(color.a);
        } else {
            color = material.albedo_factor;
        }

        if(!material.emissive_image_id.is_empty()) {
            color.rgb += Texture2D<f32vec4>::get(material.emissive_image_id).Sample(SamplerState::get(material.emissive_sampler_id), input.uv).rgb;
        }
        
        if(!material.normal_image_id.is_empty()) {
            f32vec3 normal_sample = f32vec3(0.0f);
            if(material.normal_image_compressed == 1) {
                const f32vec2 raw_normal =  Texture2D<f32vec4>::get(material.normal_image_id).Sample(SamplerState::get(material.normal_sampler_id), input.uv).xy * 2.0f - 1.0f;
                normal_sample = f32vec3(raw_normal, sqrt(clamp(1.0f - dot(raw_normal, raw_normal), 0.0, 1.0)));
            } else {
                normal_sample = Texture2D<f32vec4>::get(material.normal_image_id).Sample(SamplerState::get(material.normal_sampler_id), input.uv).rgb * 2.0f - 1.0f;
            }
            
            normal = calculate_normal_from_map(
                normal_sample,
                normalize(input.normal),
                input.position,
                input.uv
            );
        }
    }

    if(color.a < 0.5f) { discard; }

    const f32vec3 adjusted_normal = !is_front_facing ? normal : -normal;
    color.rgb = shade(
        color.rgb, 
        input.position, 
        normalize(adjusted_normal), 
        push.uses.u_globals.sun_direction, 
        push.uses.u_point_lights, 
        push.uses.u_spot_lights
    );

    return f32vec4(color.rgb, 1.0f);
}   
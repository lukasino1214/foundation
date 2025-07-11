#include "daxa/daxa.inl"
#include "draw_meshlets_transparent.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsTransparentWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_transparent_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlets_data_merged->hw_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

struct MeshPayload {
    u32 index;
};

[[vk::push_constant]] DrawMeshletsTransparentPush push;
[shader("amplification")]
[numthreads(1, 1, 1)]
void draw_meshlets_transparent_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void draw_meshlets_transparent_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<VertexTransparent, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<VisibilityPrimitiveTransparent, MAX_TRIANGLES_PER_MESHLET> out_primitives) {
    const u32 thread_index = local_thread_index.x;

    
    const MeshletInstanceData meshlet_instance_data = push.uses.u_meshlets_data_merged.hw_meshlet_data[payload.index];
    const Mesh mesh = push.uses.u_meshes[meshlet_instance_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_instance_data.meshlet_index];

    f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_instance_data.transform_index].model_matrix);
    
    const u32 meshlet_index = push.uses.u_meshlets_data_merged.hw_offset + payload.index;
    
    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);
    if(thread_index < meshlet.vertex_count) {
        const u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + thread_index];
        
        VertexTransparent vertex;
        vertex.world_position = mul(pvm, f32vec4(mesh.vertex_positions[vertex_index], 1.0f)).xyz;

        if(push.uses.u_globals.render_as_observer) {
            pvm = mul(push.uses.u_globals->observer_camera.projection_view_matrix, pvm);
        } else {
            pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, pvm);
        }

        vertex.set_position(mul(pvm, f32vec4(mesh.vertex_positions[vertex_index], 1.0f)));
        vertex.normal = normalize(decode_normal(mesh.vertex_normals[vertex_index]));
        vertex.uv = decode_uv(mesh.vertex_uvs[vertex_index]);
        out_vertices[thread_index] = vertex;
    }

    if(thread_index < meshlet.triangle_count) {
        u32vec3 indices = u32vec3(
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)
        );

        out_indices[thread_index] = indices;
        
        VisibilityPrimitiveTransparent primitive;
        primitive.set_cull_primitive(false);
        encode_triangle_id(primitive.vis_index, meshlet_index, thread_index);
        primitive.material_index = mesh.material_index;

        out_primitives[thread_index] = primitive;
    }
}

f32vec3 pixel_index_to_world_space(CameraInfo camera, f32vec2 pixel_index, f32 depth) {
    const f32vec2 ndc_xy = (pixel_index + 0.5f) * push.uses.u_globals.render_target_size_inv;
    const f32vec4 clip_space_position = f32vec4(ndc_xy * 2.0 - 1.0, depth, 1.0);
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

static f32 zMultiplier = 10.0f;
static f32 rcpWeightMultiplierZ = 1.0f / 200.0f;
static f32 alphaWeightMultiplier = 40.0f;

struct PixelOutput {
    f32vec4 color : SV_Target0;
    f32 alpha : SV_Target1;
};

[shader("fragment")]
PixelOutput draw_meshlets_transparent_frag(in VertexTransparent vertex, in VisibilityPrimitiveTransparent primitive, f32vec4 pixel_position: SV_Position) {
    f32vec4 color = f32vec4(f32vec3(0.0, 0.0, 0.0), 1.0);
    f32vec3 normal = normalize(vertex.normal);

    if(primitive.material_index != INVALID_ID) {
        daxa_ImageViewId normal_image_id = push.uses.u_materials[primitive.material_index].normal_image_id;
        if(normal_image_id.value != 0) {
            const f32vec2 raw_normal =  Texture2D<f32vec4>::get(normal_image_id).Sample(SamplerState::get(push.uses.u_materials[primitive.material_index].normal_sampler_id), vertex.uv).xy * 2.0f - 1.0f;
            const f32vec3 uncompressed_normal = f32vec3(raw_normal, sqrt(clamp(1.0f - dot(raw_normal, raw_normal), 0.0, 1.0)));

            f32vec3 p_dx = ddx_coarse(vertex.world_position);
            f32vec3 p_dy = ddx_coarse(vertex.world_position);
            f32vec2 uv_dx = ddx_coarse(vertex.uv);
            f32vec2 uv_dy = ddx_coarse(vertex.uv);

            f32vec3 tangent = normalize(uv_dy.y * p_dx - uv_dx.y * p_dy);

            const f32vec3 x = cross(normal, tangent);
            tangent = cross(x, normal);
            tangent = normalize(tangent);

            const daxa_f32mat3x3 tbn = transpose(daxa_f32mat3x3(tangent, cross(tangent, normal), normal));
            normal = normalize(mul(tbn, uncompressed_normal));
        }

        daxa_ImageViewId albedo_image_id = push.uses.u_materials[primitive.material_index].albedo_image_id;
        if(albedo_image_id.value != 0) {
            const f32vec4 albedo = Texture2D<f32vec4>::get(albedo_image_id).Sample(SamplerState::get(push.uses.u_materials[primitive.material_index].albedo_sampler_id), vertex.uv);
            color = f32vec4(albedo.rgb, 1.0);
        } else {
            color = push.uses.u_materials[primitive.material_index].albedo_factor;
        }

        daxa_ImageViewId alpha_mask_image_id = push.uses.u_materials[primitive.material_index].alpha_mask_image_id;
        if(alpha_mask_image_id.value != 0) {
            color.w = Texture2D<f32>::get(alpha_mask_image_id).Sample(SamplerState::get(push.uses.u_materials[primitive.material_index].albedo_sampler_id), vertex.uv).x;
        }
        
        daxa_ImageViewId emissive_image_id = push.uses.u_materials[primitive.material_index].emissive_image_id;
        if(emissive_image_id.value != 0) {
            color += f32vec4(Texture2D<f32vec4>::get(emissive_image_id).Sample(SamplerState::get(push.uses.u_materials[primitive.material_index].emissive_sampler_id), vertex.uv).rgb, 0);
        }
    }

    const f32 ambient = 0.1f;
    const f32vec3 world_position = pixel_index_to_world_space(push.uses.u_globals.main_camera, f32vec2(pixel_position.xy), pixel_position.z);

    {
        const SunLight* light = push.uses.u_sun;
        const f32 diffuse = max(dot(normal, -light.direction), ambient);

        color.rgb *= diffuse * light.color * light.intensity;
    }

    for(u32 i = 0; i < push.uses.u_point_lights.count; i++) {
        const PointLight light = push.uses.u_point_lights.point_lights[i];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_point_light_attenuation(to_light_dist, light.range);

        color.rgb += diffuse * light.color * light.intensity * attenuation;
    }

    for(u32 i = 0; i < push.uses.u_spot_lights.count; i++) {
        const SpotLight light = push.uses.u_spot_lights.spot_lights[i];
        const f32vec3 position_to_light = normalize(light.position - world_position);
        const f32 diffuse = max(dot(normal, position_to_light), 0.0f);

        const f32 to_light_dist = length(light.position - world_position);
        const f32 attenuation = calculate_spot_light_attenuation(position_to_light, to_light_dist, light);

        color.rgb += diffuse * light.color * light.intensity * attenuation;
    }

    color.rgb *= color.a;

    const f32 depthZ = -pixel_position.z * zMultiplier;

    const f32 distWeight = clamp(0.03 / (1e-5 + pow(depthZ * rcpWeightMultiplierZ, 4.0)), 1e-2, 3e3);

    f32 alphaWeight = min(1.0, max(max(color.r, color.g), max(color.b, color.a)) * alphaWeightMultiplier + 0.01);
    alphaWeight *= alphaWeight;

    const f32 weight = alphaWeight * distWeight;

#if OVERDRAW_DEBUG
    InterlockedAdd(push.uses.u_overdraw_image.get()[u32vec2(vertex.position.xy)], 1);
#endif

    PixelOutput pixel_output;
    pixel_output.color = color * weight;
    pixel_output.alpha = color.a;

    return pixel_output;
}   
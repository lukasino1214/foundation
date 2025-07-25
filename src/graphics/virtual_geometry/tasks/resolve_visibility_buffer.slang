#include "daxa/daxa.inl"
#include "resolve_visibility_buffer.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

struct BarycentricDeriv {
    f32vec3 m_lambda;
    f32vec3 m_ddx;
    f32vec3 m_ddy;
};

BarycentricDeriv calc_bary_and_deriv(f32vec4 pt0, f32vec4 pt1, f32vec4 pt2, f32vec2 pixelNdc, f32vec2 winSize) {
    BarycentricDeriv ret;   
    f32vec3 invW = 1.0f / (f32vec3(pt0.w, pt1.w, pt2.w)); 
    f32vec2 ndc0 = pt0.xy * invW.x;
    f32vec2 ndc1 = pt1.xy * invW.y;
    f32vec2 ndc2 = pt2.xy * invW.z;  
    f32 invDet = 1.0f / (determinant(daxa_f32mat2x2(ndc2 - ndc1, ndc0 - ndc1)));
    ret.m_ddx = f32vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
    ret.m_ddy = f32vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;
    f32 ddxSum = dot(ret.m_ddx, f32vec3(1,1,1));
    f32 ddySum = dot(ret.m_ddy, f32vec3(1,1,1));   
    f32vec2 deltaVec = pixelNdc - ndc0;
    f32 interpInvW = invW.x + deltaVec.x*ddxSum + deltaVec.y*ddySum;
    f32 interpW = 1.0f / (interpInvW);    
    ret.m_lambda.x = interpW * (invW[0] + deltaVec.x*ret.m_ddx.x + deltaVec.y*ret.m_ddy.x);
    ret.m_lambda.y = interpW * (0.0f    + deltaVec.x*ret.m_ddx.y + deltaVec.y*ret.m_ddy.y);
    ret.m_lambda.z = interpW * (0.0f    + deltaVec.x*ret.m_ddx.z + deltaVec.y*ret.m_ddy.z); 
    ret.m_ddx *= (2.0f / winSize.x);
    ret.m_ddy *= (2.0f / winSize.y);
    ddxSum    *= (2.0f / winSize.x);
    ddySum    *= (2.0f / winSize.y);  
    ret.m_ddy *= -1.0f;
    ddySum    *= -1.0f; 
    f32 interpW_ddx = 1.0f / (interpInvW + ddxSum);
    f32 interpW_ddy = 1.0f / (interpInvW + ddySum);   
    ret.m_ddx = interpW_ddx * (ret.m_lambda*interpInvW + ret.m_ddx) - ret.m_lambda;
    ret.m_ddy = interpW_ddy * (ret.m_lambda*interpInvW + ret.m_ddy) - ret.m_lambda;   
    return ret;
}

struct UvGrad {
    f32vec2 uv;
    f32vec2 ddx;
    f32vec2 ddy;
};

f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in f32[3] attributes) {
    const f32vec3 v = f32vec3(attributes[0], attributes[1], attributes[2]);
    return f32vec3(
        dot(v, derivatives.m_lambda),
        dot(v, derivatives.m_ddx),
        dot(v, derivatives.m_ddy));
}

f32vec3 visgeo_interpolate_vec3(f32vec3 derivator, f32vec3 v0, f32vec3 v1, f32vec3 v2)
{
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec3 merged_z = f32vec3(v0.z, v1.z, v2.z);
    f32vec3 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    ret.z = dot(merged_z, derivator);
    return ret;
}

f32vec2 visgeo_interpolate_vec2(f32vec3 derivator, f32vec2 v0, f32vec2 v1, f32vec2 v2)
{
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec2 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    return ret;
}

f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in f32vec3[3] attributes) {
    return
        attributes[0] * derivatives.m_lambda.x +
        attributes[1] * derivatives.m_lambda.y +
        attributes[2] * derivatives.m_lambda.z;
}

f32vec3 turbo_color_map(f32 x) {
    const f32vec4 kRedVec4 = f32vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
    const f32vec4 kGreenVec4 = f32vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
    const f32vec4 kBlueVec4 = f32vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const f32vec2 kRedVec2 = f32vec2(-152.94239396, 59.28637943);
    const f32vec2 kGreenVec2 = f32vec2(4.27729857, 2.82956604);
    const f32vec2 kBlueVec2 = f32vec2(-89.90310912, 27.34824973);

    x = clamp(x, 0, 1);
    f32vec4 v4 = f32vec4( 1.0, x, x * x, x * x * x);
    f32vec2 v2 = v4.zw * v4.z;
    return f32vec3(
      dot(v4, kRedVec4)   + dot(v2, kRedVec2),
      dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
      dot(v4, kBlueVec4)  + dot(v2, kBlueVec2)
    );
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

[[vk::push_constant]] ResolveVisibilityBufferPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void resolve_visibility_buffer(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }

    u32 vis_index = 0;
    u32 depth = 0;
    decode_visibility_buffer_payload(tex_rw_u64_table[push.uses.u_visibility_image.index()][pixel.xy], vis_index, depth);

    if(vis_index == INVALID_ID || depth == 0) {
        push.uses.u_image.get()[pixel.xy] = f32vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    u32 meshlet_data_index = 0;
    u32 triangle_index = 0;
    decode_triangle_id(vis_index, meshlet_data_index, triangle_index);

    const MeshletInstanceData meshlet_instance_data = push.uses.u_meshlets_instance_data[meshlet_data_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_instance_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_instance_data.meshlet_index];

    if(push.uses.u_globals->mouse_selection_readback.state == 1 && 
        push.uses.u_globals->mouse_selection_readback.id == INVALID_ID && 
        pixel.x == push.uses.u_globals->mouse_selection_readback.mouse_position.x && 
        pixel.y == push.uses.u_globals->mouse_selection_readback.mouse_position.y) {
        push.uses.u_globals->mouse_selection_readback.id = meshlet_instance_data.transform_index;
        push.uses.u_mouse_selection_readback->id = meshlet_instance_data.transform_index;
    }

    const u32[3] indices = u32[3](
        get_vertex_index(mesh, meshlet, triangle_index, 0),
        get_vertex_index(mesh, meshlet, triangle_index, 1),
        get_vertex_index(mesh, meshlet, triangle_index, 2)
    );

    const f32vec3[3] positions = f32vec3[3](
        mesh.vertex_positions[indices[0]],
        mesh.vertex_positions[indices[1]],
        mesh.vertex_positions[indices[2]]
    );

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_instance_data.transform_index].model_matrix); 
    const f32vec4[3] world_positions = f32vec4[3](
        mul(transform_matrix, f32vec4(positions[0], 1.0)),
        mul(transform_matrix, f32vec4(positions[1], 1.0)),
        mul(transform_matrix, f32vec4(positions[2], 1.0))
    );

    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;

    const f32vec4[3] clip_position = f32vec4[3](
        mul(projection_view_matrix, world_positions[0]),
        mul(projection_view_matrix, world_positions[1]),
        mul(projection_view_matrix, world_positions[2])
    ); 

    const f32vec2 ndc_xy = ((pixel.xy + 0.5f) * push.uses.u_globals->render_target_size_inv) * 2.0f - 1.0f;
    const BarycentricDeriv derivatives = calc_bary_and_deriv(clip_position[0], clip_position[1], clip_position[2], ndc_xy, f32vec2(push.uses.u_globals->render_target_size));

    const f32vec2[3] uvs = f32vec2[3](
        decode_uv(mesh.vertex_uvs[indices[0]]),
        decode_uv(mesh.vertex_uvs[indices[1]]),
        decode_uv(mesh.vertex_uvs[indices[2]])
    );

    const f32vec3[2] interp_uv = f32vec3[2](
        interpolate_attributes(derivatives, f32[](uvs[0].x, uvs[1].x, uvs[2].x)),
        interpolate_attributes(derivatives, f32[](uvs[0].y, uvs[1].y, uvs[2].y))
    );

    const UvGrad uv_grad = UvGrad(
        f32vec2(interp_uv[0].x, interp_uv[1].x),
        f32vec2(interp_uv[0].y, interp_uv[1].y),
        f32vec2(interp_uv[0].z, interp_uv[1].z)
    );

    const f32vec3[3] vertex_normals = f32vec3[3](
        decode_normal(mesh.vertex_normals[indices[0]]),
        decode_normal(mesh.vertex_normals[indices[1]]),
        decode_normal(mesh.vertex_normals[indices[2]])
    );

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

    f32vec3 normal = normalize(interpolate_attributes(
        derivatives, f32vec3[3](
            worldspace_vertex_normals[0].xyz,
            worldspace_vertex_normals[1].xyz,
            worldspace_vertex_normals[2].xyz
        )
    ));

    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(meshlet_instance_data.meshlet_index) * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    if(mesh.material_index != INVALID_ID) {
        daxa_ImageViewId normal_image_id = push.uses.u_materials[mesh.material_index].normal_image_id;
        if(normal_image_id.value != 0) {
            const f32vec2 raw_normal =  Texture2D<f32vec4>::get(normal_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].normal_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).xy * 2.0f - 1.0f;
            const f32vec3 uncompressed_normal = f32vec3(raw_normal, sqrt(clamp(1.0f - dot(raw_normal, raw_normal), 0.0, 1.0)));

            const f32vec3 p_dx = visgeo_interpolate_vec3(
                derivatives.m_ddx, 
                world_positions[0].xyz,
                world_positions[1].xyz,
                world_positions[2].xyz
            );
            const f32vec3 p_dy = visgeo_interpolate_vec3(
                derivatives.m_ddy, 
                world_positions[0].xyz,
                world_positions[1].xyz,
                world_positions[2].xyz
            );

            f32vec3 tangent = normalize(uv_grad.ddy.y * p_dx - uv_grad.ddx.y * p_dy);

            const f32vec3 x = cross(normal, tangent);
            tangent = cross(x, normal);
            tangent = normalize(tangent);

            const daxa_f32mat3x3 tbn = transpose(daxa_f32mat3x3(tangent, cross(tangent, normal), normal));
            normal = normalize(mul(tbn, uncompressed_normal));
        }

        daxa_ImageViewId albedo_image_id = push.uses.u_materials[mesh.material_index].albedo_image_id;
        if(albedo_image_id.value != 0) {
            const f32vec4 albedo = Texture2D<f32vec4>::get(albedo_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].albedo_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy);
            color = f32vec4(albedo.rgb, 1.0);
        } else {
            color = push.uses.u_materials[mesh.material_index].albedo_factor;
        }
        
        daxa_ImageViewId emissive_image_id = push.uses.u_materials[mesh.material_index].emissive_image_id;
        if(emissive_image_id.value != 0) {
            color += f32vec4(Texture2D<f32vec4>::get(emissive_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].emissive_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).rgb, 0);
        }

        // texture streaming
        const f32 mip_map = log2(max(length(uv_grad.ddx * 65536u), length(uv_grad.ddy * 65536u)));
        const u32 wave_mask = WaveActiveBitOr(65536u >> u32(max(0, mip_map)));
        if(WaveIsFirstLane()) {
            InterlockedOr(push.uses.u_readback_material[mesh.material_index], wave_mask);
        }
    }
    
    // color = f32vec4(0.0, 0.0, 0.0, 1.0);
    // daxa_ImageViewId roughness_image_id = push.uses.u_materials[mesh.material_index].roughness_image_id;
    // if(roughness_image_id.value != 0) {
    //     color.r = Texture2D<f32>::get(roughness_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].roughness_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).r;
    // }
    
    // daxa_ImageViewId metalness_image_id = push.uses.u_materials[mesh.material_index].metalness_image_id;
    // if(metalness_image_id.value != 0) {
        //     color.g = Texture2D<f32>::get(metalness_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].metalness_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).r;
        // }
        
    // color = f32vec4(normal * 0.5 + 0.5, 1.0f);

#if OVERDRAW_DEBUG
    color.rgb = turbo_color_map(f32(push.uses.u_overdraw_image.get()[pixel.xy]) / 24.0);
#endif

    const f32 ambient = 0.1f;
    const f32vec3 world_position = pixel_index_to_world_space(push.uses.u_globals.main_camera, f32vec2(pixel.xy), asfloat(depth));

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

    push.uses.u_image.get()[pixel.xy] = color;
}
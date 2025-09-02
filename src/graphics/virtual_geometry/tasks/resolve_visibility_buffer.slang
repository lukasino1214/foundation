#include "daxa/daxa.inl"
#include "resolve_visibility_buffer.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/shading.slang"

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

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

[[vk::push_constant]] ResolveVisibilityBufferPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void resolve_visibility_buffer(u32vec3 pixel : SV_DispatchThreadID, u32vec2 tile_index : SV_GroupID) {
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

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh.mesh_group_index].model_matrix); 
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

            f32 r = 1.0f / (uv_grad.ddx.x * uv_grad.ddy.y - uv_grad.ddx.y * uv_grad.ddy.x);
            f32vec3 tangent = normalize((p_dx * uv_grad.ddy.y - p_dy * uv_grad.ddx.y) * r);
            f32vec3 bitangent = normalize((-p_dx * uv_grad.ddy.y + p_dy * uv_grad.ddx.y) * r);

            const f32mat3x3 tbn = transpose(daxa_f32mat3x3(tangent, bitangent, normal));
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

    const f32vec3 world_position = pixel_index_to_world_space(push.uses.u_globals.main_camera, push.uses.u_globals.render_target_size_inv, f32vec2(pixel.xy), asfloat(depth));
    f32 occlusion = push.uses.u_ssao_image.get()[pixel.xy / 2];

    const u32vec2 resolution = push.uses.u_globals.render_target_size;
    const u32vec2 tile_frustums = u32vec2(round_up_div(resolution.x, 16), round_up_div(resolution.y, 16));
    u32 flattened_tile_index = tile_index.y * tile_frustums.x + tile_index.x;
    TileData* tile_data = push.uses.u_tile_data + flattened_tile_index;

    color.rgb = occlusion * shade_tiled(
        color.rgb, 
        world_position, 
        normal, 
        tile_data, 
        push.uses.u_sun, 
        push.uses.u_point_lights, 
        push.uses.u_spot_lights, 
        push.uses.u_tile_indices + 2 * flattened_tile_index * MAX_LIGHT_BITMASK_SIZE + MAX_POINT_LIGHT_BITMASK_SIZE, 
        push.uses.u_tile_indices + 2 * flattened_tile_index * MAX_LIGHT_BITMASK_SIZE + MAX_POINT_LIGHT_BITMASK_SIZE + MAX_SPOT_LIGHT_BITMASK_SIZE + MAX_POINT_LIGHT_BITMASK_SIZE
    );

#if OVERDRAW_DEBUG
    color.rgb = f32vec3(f32(push.uses.u_overdraw_image.get()[pixel.xy]) / 24.0);
#endif

    // if(push.uses.u_overdraw_image.get()[pixel.xy] != 0) {
    //     color.rgb = lerp(color.rgb, turbo_color_map(f32(push.uses.u_overdraw_image.get()[pixel.xy]) / 512.0), 0.5);
    // }

    push.uses.u_image.get()[pixel.xy] = color;
}
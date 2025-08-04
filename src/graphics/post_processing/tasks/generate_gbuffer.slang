#include "daxa/daxa.inl"
#include "generate_gbuffer.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

groupshared u32 shared_normals[8][8];
groupshared u32 shared_detail_normals[8][8];
groupshared f32 shared_depths[8][8];

[[vk::push_constant]] GenerateGBufferPush push;
[shader("compute")]
[numthreads(8, 8, 1)]
void generate_gbuffer(u32vec3 pixel : SV_DispatchThreadID, u32vec2 in_group_pixel : SV_GroupThreadID) {
    u32 vis_index = INVALID_ID;
    u32 depth = 0;

    if(all(lessThan(pixel.xy, deref(push.uses.u_globals).render_target_size))) {
        decode_visibility_buffer_payload(tex_rw_u64_table[push.uses.u_visibility_image.index()][pixel.xy], vis_index, depth);
    }

    if(vis_index != INVALID_ID) { 
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

        f32vec3 detail_normal = normal;
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
                detail_normal = normalize(mul(tbn, uncompressed_normal));
            }
        }

        u32 encoded_normal = encode_normal(normal);
        u32 encoded_detail_normal = encode_normal(detail_normal);

        push.uses.u_normal_image.get()[pixel.xy] = encoded_normal;
        push.uses.u_normal_detail_image.get()[pixel.xy] = encoded_detail_normal;

        shared_normals[in_group_pixel.x][in_group_pixel.y] = encoded_normal;
        shared_detail_normals[in_group_pixel.x][in_group_pixel.y] = encoded_detail_normal;
        shared_depths[in_group_pixel.x][in_group_pixel.y] = asfloat(depth);
    } else {
        shared_normals[in_group_pixel.x][in_group_pixel.y] = 0;
        shared_detail_normals[in_group_pixel.x][in_group_pixel.y] = 0;
        shared_depths[in_group_pixel.x][in_group_pixel.y] = 0.0f;
    }

    GroupMemoryBarrierWithGroupSync();

    if (all((pixel.xy & u32vec2(1,1)) == u32vec2(0,0))) {
        u32vec2 half_index = pixel.xy / 2;

        f32vec4 depths = {
            shared_depths[in_group_pixel.x + 0][in_group_pixel.y + 0],
            shared_depths[in_group_pixel.x + 1][in_group_pixel.y + 0],
            shared_depths[in_group_pixel.x + 0][in_group_pixel.y + 1],
            shared_depths[in_group_pixel.x + 1][in_group_pixel.y + 1]
        };

        u32vec4 normals = {
            shared_normals[in_group_pixel.x + 0][in_group_pixel.y + 0],
            shared_normals[in_group_pixel.x + 1][in_group_pixel.y + 0],
            shared_normals[in_group_pixel.x + 0][in_group_pixel.y + 1],
            shared_normals[in_group_pixel.x + 1][in_group_pixel.y + 1]
        };

        u32vec4 detail_normals = {
            shared_detail_normals[in_group_pixel.x + 0][in_group_pixel.y + 0],
            shared_detail_normals[in_group_pixel.x + 1][in_group_pixel.y + 0],
            shared_detail_normals[in_group_pixel.x + 0][in_group_pixel.y + 1],
            shared_detail_normals[in_group_pixel.x + 1][in_group_pixel.y + 1]
        };

        f32 closest_depth = 0.0f;
        u32 closest_normal = 0u;
        u32 closest_detail_normal = 0u;
        for (u32 i = 0; i < 4; i++) {
            if (depths[i] > closest_depth) {
                closest_depth = depths[i];
                closest_normal = normals[i];
                closest_detail_normal = detail_normals[i];
            }
        }

        push.uses.u_depth_half_res_image.get()[half_index] = closest_depth;
        push.uses.u_normal_half_res_image.get()[half_index] = closest_normal;
        push.uses.u_normal_detail_half_res_image.get()[half_index] = closest_detail_normal;
    }
}
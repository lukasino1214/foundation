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
    daxa_f32vec3 m_lambda;
    daxa_f32vec3 m_ddx;
    daxa_f32vec3 m_ddy;
};

BarycentricDeriv calc_bary_and_deriv(daxa_f32vec4 pt0, daxa_f32vec4 pt1, daxa_f32vec4 pt2, daxa_f32vec2 pixelNdc, daxa_f32vec2 winSize) {
    BarycentricDeriv ret;   
    daxa_f32vec3 invW = 1.0f / (daxa_f32vec3(pt0.w, pt1.w, pt2.w)); 
    daxa_f32vec2 ndc0 = pt0.xy * invW.x;
    daxa_f32vec2 ndc1 = pt1.xy * invW.y;
    daxa_f32vec2 ndc2 = pt2.xy * invW.z;  
    float invDet = 1.0f / (determinant(daxa_f32mat2x2(ndc2 - ndc1, ndc0 - ndc1)));
    ret.m_ddx = daxa_f32vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
    ret.m_ddy = daxa_f32vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;
    float ddxSum = dot(ret.m_ddx, daxa_f32vec3(1,1,1));
    float ddySum = dot(ret.m_ddy, daxa_f32vec3(1,1,1));   
    daxa_f32vec2 deltaVec = pixelNdc - ndc0;
    float interpInvW = invW.x + deltaVec.x*ddxSum + deltaVec.y*ddySum;
    float interpW = 1.0f / (interpInvW);    
    ret.m_lambda.x = interpW * (invW[0] + deltaVec.x*ret.m_ddx.x + deltaVec.y*ret.m_ddy.x);
    ret.m_lambda.y = interpW * (0.0f    + deltaVec.x*ret.m_ddx.y + deltaVec.y*ret.m_ddy.y);
    ret.m_lambda.z = interpW * (0.0f    + deltaVec.x*ret.m_ddx.z + deltaVec.y*ret.m_ddy.z); 
    ret.m_ddx *= (2.0f/winSize.x);
    ret.m_ddy *= (2.0f/winSize.y);
    ddxSum    *= (2.0f/winSize.x);
    ddySum    *= (2.0f/winSize.y);  
    ret.m_ddy *= -1.0f;
    ddySum    *= -1.0f; 
    float interpW_ddx = 1.0f / (interpInvW + ddxSum);
    float interpW_ddy = 1.0f / (interpInvW + ddySum);   
    ret.m_ddx = interpW_ddx*(ret.m_lambda*interpInvW + ret.m_ddx) - ret.m_lambda;
    ret.m_ddy = interpW_ddy*(ret.m_lambda*interpInvW + ret.m_ddy) - ret.m_lambda;   
    return ret;
}

struct UvGrad {
    daxa_f32vec2 uv;
    daxa_f32vec2 ddx;
    daxa_f32vec2 ddy;
};

daxa_f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in float[3] attributes) {
    const f32vec3 v = f32vec3(attributes[0], attributes[1], attributes[2]);
    return f32vec3(
        dot(v, derivatives.m_lambda),
        dot(v, derivatives.m_ddx),
        dot(v, derivatives.m_ddy));
}

daxa_f32vec3 visgeo_interpolate_vec3(daxa_f32vec3 derivator, daxa_f32vec3 v0, daxa_f32vec3 v1, daxa_f32vec3 v2)
{
    daxa_f32vec3 merged_x = daxa_f32vec3(v0.x, v1.x, v2.x);
    daxa_f32vec3 merged_y = daxa_f32vec3(v0.y, v1.y, v2.y);
    daxa_f32vec3 merged_z = daxa_f32vec3(v0.z, v1.z, v2.z);
    daxa_f32vec3 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    ret.z = dot(merged_z, derivator);
    return ret;
}

daxa_f32vec2 visgeo_interpolate_vec2(daxa_f32vec3 derivator, daxa_f32vec2 v0, daxa_f32vec2 v1, daxa_f32vec2 v2)
{
    daxa_f32vec3 merged_x = daxa_f32vec3(v0.x, v1.x, v2.x);
    daxa_f32vec3 merged_y = daxa_f32vec3(v0.y, v1.y, v2.y);
    daxa_f32vec2 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    return ret;
}

daxa_f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in f32vec3[3] attributes) {
    return
        attributes[0] * derivatives.m_lambda.x +
        attributes[1] * derivatives.m_lambda.y +
        attributes[2] * derivatives.m_lambda.z;
}

[[vk::push_constant]] ResolveVisibilityBufferPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void main(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }

    daxa_u32 vis_index = 0;
    daxa_u32 depth = 0;
    decode_visibility_buffer_payload(tex_rw_u64_table[push.uses.u_visibility_image.index()][pixel.xy], vis_index, depth);

    if(vis_index == INVALID_ID || depth == 0) {
        RWTexture2D<f32vec4> image = RWTexture2D<f32>::get(push.uses.u_image);
        image[pixel.xy] = f32vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    daxa_u32 meshlet_data_index = 0;
    daxa_u32 triangle_index = 0;
    decode_triangle_id(vis_index, meshlet_data_index, triangle_index);

    const MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[meshlet_data_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

    const daxa_u32[3] indices = daxa_u32[3](
        get_vertex_index(mesh, meshlet, triangle_index, 0),
        get_vertex_index(mesh, meshlet, triangle_index, 1),
        get_vertex_index(mesh, meshlet, triangle_index, 2)
    );

    const daxa_f32vec3[3] positions = f32vec3[3](
        mesh.vertex_positions[indices[0]],
        mesh.vertex_positions[indices[1]],
        mesh.vertex_positions[indices[2]]
    );

    const daxa_f32mat4x4 transform_matrix = push.uses.u_transforms[meshlet_data.transform_index].model_matrix; 
    const daxa_f32vec4[3] world_positions = f32vec4[3](
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

    const daxa_f32vec2 ndc_xy = ((pixel.xy + 0.5f) * push.uses.u_globals->render_target_size_inv) * 2.0f - 1.0f;
    const BarycentricDeriv derivatives = calc_bary_and_deriv(clip_position[0], clip_position[1], clip_position[2], ndc_xy, f32vec2(push.uses.u_globals->render_target_size));

    const daxa_f32vec2[3] uvs = daxa_f32vec2[3](
        decode_uv(mesh.vertex_uvs[indices[0]]),
        decode_uv(mesh.vertex_uvs[indices[1]]),
        decode_uv(mesh.vertex_uvs[indices[2]])
    );

    const f32vec3[2] interp_uv = f32vec3[2](
        interpolate_attributes(derivatives, float[](uvs[0].x, uvs[1].x, uvs[2].x)),
        interpolate_attributes(derivatives, float[](uvs[0].y, uvs[1].y, uvs[2].y))
    );

    const UvGrad uv_grad = UvGrad(
        f32vec2(interp_uv[0].x, interp_uv[1].x),
        f32vec2(interp_uv[0].y, interp_uv[1].y),
        f32vec2(interp_uv[0].z, interp_uv[1].z)
    );

    const daxa_f32vec3[3] vertex_normals = daxa_f32vec3[3](
        decode_normal(mesh.vertex_normals[indices[0]]),
        decode_normal(mesh.vertex_normals[indices[1]]),
        decode_normal(mesh.vertex_normals[indices[2]])
    );

    const daxa_f32mat4x4 normal_matrix = push.uses.u_transforms[meshlet_data.transform_index].normal_matrix; 
    const daxa_f32vec4[3] worldspace_vertex_normals = daxa_f32vec4[3](
        mul(normal_matrix, daxa_f32vec4(vertex_normals[0], 0)),
        mul(normal_matrix, daxa_f32vec4(vertex_normals[1], 0)),
        mul(normal_matrix, daxa_f32vec4(vertex_normals[2], 0))
    );

    f32vec3 normal = normalize(interpolate_attributes(
        derivatives, f32vec3[3](
            worldspace_vertex_normals[0].xyz,
            worldspace_vertex_normals[1].xyz,
            worldspace_vertex_normals[2].xyz
        )
    ));

    daxa_ImageViewId normal_image_id = push.uses.u_materials[mesh.material_index].normal_image_id;
    if(normal_image_id.value != 0) {
        const f32vec2 raw_normal =  Texture2D<float>::get(normal_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].normal_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).xy * 2.0f - 1.0f;
        const f32vec3 uncompressed_normal = f32vec3(raw_normal, sqrt(clamp(1.0f - dot(raw_normal, raw_normal), 0.0, 1.0)));

        const daxa_f32vec3 p_dx = visgeo_interpolate_vec3(
            derivatives.m_ddx, 
            world_positions[0].xyz,
            world_positions[1].xyz,
            world_positions[2].xyz
        );
        const daxa_f32vec3 p_dy = visgeo_interpolate_vec3(
            derivatives.m_ddy, 
            world_positions[0].xyz,
            world_positions[1].xyz,
            world_positions[2].xyz
        );

        daxa_f32vec3 tangent = normalize(uv_grad.ddy.y * p_dx - uv_grad.ddx.y * p_dy);

        const daxa_f32vec3 x = cross(normal, tangent);
        tangent = cross(x, normal);
        tangent = normalize(tangent);

        const daxa_f32mat3x3 tbn = transpose(daxa_f32mat3x3(tangent, cross(tangent, normal), normal));
        normal = normalize(mul(tbn, uncompressed_normal));
    }

    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(meshlet_data.meshlet_index) * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    daxa_ImageViewId albedo_image_id = push.uses.u_materials[mesh.material_index].albedo_image_id;
    if(albedo_image_id.value != 0) {
        const f32vec4 albedo = Texture2D<float>::get(albedo_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].albedo_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy);
        color = f32vec4(albedo.rgb, 1.0);
    }

    daxa_ImageViewId alpha_mask_image_id = push.uses.u_materials[mesh.material_index].alpha_mask_image_id;
    if(alpha_mask_image_id.value != 0) {
        const f32 alpha = Texture2D<float>::get(alpha_mask_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].alpha_mask_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).x;
        if(push.uses.u_materials[mesh.material_index].alpha_mode == 1) {
            if(alpha < push.uses.u_materials[mesh.material_index].alpha_cutoff) { color = f32vec4(0.0, 0.05, 0.0, 1.0); }
        }
    }
    
    daxa_ImageViewId emissive_image_id = push.uses.u_materials[mesh.material_index].emissive_image_id;
    if(emissive_image_id.value != 0) {
        color += f32vec4(Texture2D<float>::get(emissive_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].emissive_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).rgb, 0);
    }

    // texture streaming
    const f32 mip_map = log2(max(length(uv_grad.ddx * 65536u), length(uv_grad.ddy * 65536u)));
    const u32 wave_mask = WaveActiveBitOr(65536u >> u32(max(0, mip_map)));
	if(WaveIsFirstLane()) {
        InterlockedOr(push.uses.u_readback_material[mesh.material_index], wave_mask);
	}
    
    
    // color = f32vec4(0.0, 0.0, 0.0, 1.0);
    // daxa_ImageViewId roughness_image_id = push.uses.u_materials[mesh.material_index].roughness_image_id;
    // if(roughness_image_id.value != 0) {
    //     color.r = Texture2D<float>::get(roughness_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].roughness_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).r;
    // }
    
    // daxa_ImageViewId metalness_image_id = push.uses.u_materials[mesh.material_index].metalness_image_id;
    // if(metalness_image_id.value != 0) {
        //     color.g = Texture2D<float>::get(metalness_image_id).SampleGrad(SamplerState::get(push.uses.u_materials[mesh.material_index].metalness_sampler_id), uv_grad.uv, uv_grad.ddx, uv_grad.ddy).r;
        // }
        
    RWTexture2D<f32vec4> image = RWTexture2D<f32>::get(push.uses.u_image);
    // color = f32vec4(normal * 0.5 + 0.5, 1.0f);
    image[pixel.xy] = color;
}
#include "daxa/daxa.inl"
#include "render_meshes.inl"
#include "../../../shader_library/common/normal_mapping.slang"

struct VertexStageOutput {
    Vertex vertex : Vertex;
    f32vec4 sv_position : SV_Position;
};

[[vk::push_constant]] RenderMeshesPush push;
[shader("vertex")]
VertexStageOutput vertex_main(u32 index: SV_VertexID) {
    VertexStageOutput output;
    output.vertex = push.vertices[index];
    output.vertex.normal = mul(deref(push.transform).normal_matrix, f32vec4(output.vertex.normal, 1)).xyz;
    output.sv_position = mul(deref(push.uses.u_globals).camera_projection_matrix,
                         mul(deref(push.uses.u_globals).camera_view_matrix,
                         mul(deref(push.transform).model_matrix,
                         f32vec4(push.vertices[index].position, 1.0))));
    return output;
}

[shader("fragment")]
f32vec4 fragment_main(Vertex vertex : Vertex) : SV_Target {
    f32vec3 normal = normalize(vertex.normal);
    daxa_ImageViewId normal_texture_id = deref(push.material).normal_texture_id;
    if(normal_texture_id.value != 0) {
        f32vec3 sampled_normal = Texture2D<float>::get(normal_texture_id).Sample(SamplerState::get(deref(push.material).normal_sampler_id), vertex.uv).xyz * 2.0 - 1.0;

        float3 dp1 = ddx(vertex.position);
        float3 dp2 = ddy(vertex.position);
        float2 duv1 = ddx(vertex.uv);
        float2 duv2 = ddy(vertex.uv);

        float3x3 M = float3x3(dp1, dp2, cross(dp1, dp2));
        float2x3 inverseM = float2x3(cross(M[1], M[2]), cross(M[2], M[0]));
        float3 t = normalize(mul(float2(duv1.x, duv2.x), inverseM));
        float3 b = normalize(mul(float2(duv1.y, duv2.y), inverseM));
        
        daxa_f32mat3x3 tbn = transpose(daxa_f32mat3x3(t, b, normal));
        normal = normalize(mul(tbn, sampled_normal));
    }

    f32vec4 color = f32vec4(normal * 0.5 + 0.5, 1.0);
    daxa_ImageViewId albedo_texture_id = deref(push.material).albedo_texture_id;
    if(albedo_texture_id.value != 0) {
        f32vec4 albedo = Texture2D<float>::get(albedo_texture_id).Sample(SamplerState::get(deref(push.material).albedo_sampler_id), vertex.uv);
        // if(deref(push.material).alpha_mode == 1 && albedo.a < deref(push.material).alpha_cutoff) { discard; }
        if(deref(push.material).alpha_mode == 1) {
            f32 alpha = Texture2D<float>::get(albedo_texture_id).SampleLevel(SamplerState::get(deref(push.material).albedo_sampler_id), vertex.uv, 0).a;
            if(alpha < deref(push.material).alpha_cutoff) { discard; }
        }
        color = f32vec4(albedo.rgb, 1.0);
    }
    daxa_ImageViewId emissive_texture_id = deref(push.material).emissive_texture_id;
    if(emissive_texture_id.value != 0) {
        color += f32vec4(Texture2D<float>::get(emissive_texture_id).Sample(SamplerState::get(deref(push.material).emissive_sampler_id), vertex.uv).rgb, 0);
    }
    
    return f32vec4(color.rgb, 1.0);
}   
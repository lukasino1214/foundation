#include "daxa/daxa.inl"
#include "render_meshes.inl"

struct VertexStageOutput {
    Vertex vertex : Vertex;
    f32vec4 sv_position : SV_Position;
};

[[vk::push_constant]] RenderMeshesPush push;
[shader("vertex")]
VertexStageOutput vertex_main(u32 index: SV_VertexID) {
    VertexStageOutput output;
    output.vertex = push.vertices[index];
    output.sv_position = mul(deref(push.uses.u_globals).camera_projection_matrix,
                         mul(deref(push.uses.u_globals).camera_view_matrix,
                         mul(deref(push.transform).model_matrix,
                         f32vec4(push.vertices[index].position, 1.0))));
    return output;
}

[shader("fragment")]
f32vec4 fragment_main(Vertex vertex : Vertex) : SV_Target {
    f32vec4 color = f32vec4(vertex.normal * 0.5 + 0.5, 1.0);
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
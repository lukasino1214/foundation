#include "daxa/daxa.inl"
#include "depth_prepass.inl"

[[vk::push_constant]] DepthPrepassPush push;

struct VertexStageOutput {
    f32vec4 sv_position : SV_Position;
    f32vec2 uv;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID) {
    const MeshGeometryData mesh = push.uses.u_meshes[push.mesh_index];

    VertexStageOutput output;
    f32vec4 world_pos = mul(mat_4x3_to_4x4(push.matrix), f32vec4(mesh.positions[vertex_index], 1.0f));
    output.sv_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, world_pos);
    
    if(mesh.uvs != 0) {
        output.uv = mesh.uvs[vertex_index];
    } else {
        output.uv = f32vec2(0.0f, 0.0f);
    }

    return output;
}

[shader("fragment")]
void fragment_main(in VertexStageOutput input) {
    const MeshGeometryData mesh = push.uses.u_meshes[push.mesh_index];

    if(mesh.material_index != INVALID_ID) {
        const Material material = push.uses.u_materials[mesh.material_index];

        f32 alpha = material.albedo_factor.a;
        if(!material.albedo_image_id.is_empty()) {
            alpha = Texture2D<f32vec4>::get(material.albedo_image_id).Sample(SamplerState::get(material.albedo_sampler_id), input.uv).a;
            f32 lod = Texture2D<f32vec4>::get(material.albedo_image_id).CalculateLevelOfDetail(SamplerState::get(material.albedo_sampler_id), input.uv);
            alpha *= 1.0f + max(0.0f, lod) * 0.25f;
            alpha = saturate(alpha);
        }

        if(alpha < 0.5f) { discard; }
    }
}

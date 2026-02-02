#include "daxa/daxa.inl"
#include "draw_mesh.inl"

[[vk::push_constant]] DrawMeshPush push;

struct VertexStageOutput {
    f32vec4 sv_position : SV_Position;
    f32vec3 normal;
    f32vec2 uv;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID) {
    MeshGeometryData mesh = push.uses.u_meshes[push.mesh_index];

    VertexStageOutput output;
    output.sv_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, mul(mat_4x3_to_4x4(push.matrix),  f32vec4(mesh.positions[vertex_index], 1.0f)));
    output.normal = mesh.normals[vertex_index];
    if(mesh.uvs != 0) {
        output.uv = mesh.uvs[vertex_index];
    }

    return output;
}

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

[shader("fragment")]
f32vec4 fragment_main(u32 primitive_id : SV_PrimitiveID, in VertexStageOutput input) : SV_Target {
    MeshGeometryData mesh = push.uses.u_meshes[push.mesh_index];
    
    f32vec4 color = f32vec4(hsv_to_rgb(f32vec3(f32(primitive_id) / 128 * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
    if(mesh.material_index != INVALID_ID) {
        Material material = push.uses.u_materials[mesh.material_index];

        if(!material.albedo_image_id.is_empty()) {
            color = Texture2D<f32vec4>::get(material.albedo_image_id).Sample(SamplerState::get(material.albedo_sampler_id), input.uv);
        } else {
            color = material.albedo_factor;
        }

        if(!material.emissive_image_id.is_empty()) {
            color.rgb += Texture2D<f32vec4>::get(material.emissive_image_id).Sample(SamplerState::get(material.emissive_sampler_id), input.uv).rgb;
        }
    }

    if(color.a < 0.5f) { discard; }
    return f32vec4(color.rgb, 1.0f);
}   
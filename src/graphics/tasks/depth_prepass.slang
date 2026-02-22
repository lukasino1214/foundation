#include "daxa/daxa.inl"
#include "depth_prepass.inl"

[[vk::push_constant]] DepthPrepassPush push;

struct MeshStageOutput {
    f32vec4 sv_position : SV_Position;
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

    const MeshGeometryData mesh = push.uses.u_mesh_geometry_data[push.mesh_index];
    const Meshlet meshlet = mesh.meshlets[group_index.x];

    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

    if(thread_index < meshlet.vertex_count) {
        const u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + thread_index];

        MeshStageOutput vertex;
        const f32vec4 world_position = mul(mat_4x3_to_4x4(push.uses.u_transforms[push.mesh_group_index]), f32vec4(mesh.positions[vertex_index], 1.0f));
        vertex.sv_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, world_position);

        if(mesh.uvs != 0) {
            vertex.uv = mesh.uvs[vertex_index];
        } else {
            vertex.uv = f32vec2(0.0f, 0.0f);
        }

        out_vertices[thread_index] = vertex;
    }

    if(thread_index < meshlet.triangle_count) {
        u32vec3 indices = u32vec3(
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)
        );

        out_indices[thread_index] = indices;
    }
}

[shader("fragment")]
void fragment_main(in MeshStageOutput input) {
    const MeshGeometryData mesh = push.uses.u_mesh_geometry_data[push.mesh_index];

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

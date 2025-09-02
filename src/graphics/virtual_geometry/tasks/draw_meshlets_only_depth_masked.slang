#include "daxa/daxa.inl"
#include "draw_meshlets_only_depth_masked.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsOnlyDepthMaskedWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_only_depth_masked_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlets_data_merged->hw_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

[[vk::push_constant]] DrawMeshletsOnlyDepthMaskedPush push;
struct MeshPayload {
    u32 index;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void draw_meshlets_only_depth_masked_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void draw_meshlets_only_depth_masked_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<VertexMasked, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<VisibilityPrimitiveMasked, MAX_TRIANGLES_PER_MESHLET> out_primitives) {
    const u32 thread_index = local_thread_index.x;
    
    const MeshletInstanceData meshlet_instance_data = push.uses.u_meshlets_data_merged.hw_meshlet_data[payload.index];
    const Mesh mesh = push.uses.u_meshes[meshlet_instance_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_instance_data.meshlet_index];

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh.mesh_group_index].model_matrix);
    const f32mat4x4 pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, transform_matrix);

    const u32 meshlet_index = push.uses.u_meshlets_data_merged.hw_offset + payload.index;

    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);
    if(thread_index < meshlet.vertex_count) {
        const u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + thread_index];

        VertexMasked vertex;
        vertex.set_position(mul(pvm, f32vec4(mesh.vertex_positions[vertex_index], 1.0f)));
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
        
        VisibilityPrimitiveMasked primitive;
        primitive.set_cull_primitive(false);
        encode_triangle_id(primitive.vis_index, meshlet_index, thread_index);
        primitive.material_index = mesh.material_index;

        out_primitives[thread_index] = primitive;
    }
}

[shader("fragment")]
void draw_meshlets_only_depth_masked_frag(in VertexMasked vertex, in VisibilityPrimitiveMasked primitive) {
    if(primitive.material_index != INVALID_ID) {
        daxa_ImageViewId alpha_mask_image_id = push.uses.u_materials[primitive.material_index].alpha_mask_image_id;
        if(alpha_mask_image_id.value != 0) {
            const f32 alpha = Texture2D<f32vec4>::get(alpha_mask_image_id).Sample(SamplerState::get(push.uses.u_materials[primitive.material_index].alpha_mask_sampler_id), vertex.uv).x;
            if(push.uses.u_materials[primitive.material_index].alpha_mode == 1) {
                if(push.uses.u_materials[primitive.material_index].alpha_cutoff > alpha) { 
                    discard; 
                }
            }
        }
    }
}   
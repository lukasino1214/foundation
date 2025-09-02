#include "daxa/daxa.inl"
#include "draw_meshlets.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlets_data_merged->hw_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

struct MeshPayload {
    u32 index;
};

[[vk::push_constant]] DrawMeshletsPush push;
[shader("amplification")]
[numthreads(1, 1, 1)]
void draw_meshlets_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void draw_meshlets_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<VisibilityPrimitive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {
    const MeshletInstanceData meshlet_instance_data = push.uses.u_meshlets_data_merged.hw_meshlet_data[payload.index];
    const Mesh mesh = push.uses.u_meshes[meshlet_instance_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_instance_data.meshlet_index];

    f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[mesh.mesh_group_index].model_matrix);
    if(push.uses.u_globals.render_as_observer) {
        pvm = mul(push.uses.u_globals->observer_camera.projection_view_matrix, pvm);
    } else {
        pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, pvm);
    }

    const u32 meshlet_index = push.uses.u_meshlets_data_merged.hw_offset + payload.index;

    generic_mesh(
        local_thread_index.x,
        meshlet_index,
        mesh,
        meshlet,
        pvm,
        out_indices,
        out_vertices,
        out_primitives
    );
}

[shader("fragment")]
void draw_meshlets_frag(in Vertex vertex, in VisibilityPrimitive primitive) {
    AtomicMaxU64(tex_rw_u64_table[push.uses.u_visibility_image.index()][u32vec2(vertex.position.xy)], encode_visibility_buffer_payload(primitive.vis_index, vertex.position.z));
#if OVERDRAW_DEBUG
    InterlockedAdd(push.uses.u_overdraw_image.get()[u32vec2(vertex.position.xy)], 1);
#endif
}   
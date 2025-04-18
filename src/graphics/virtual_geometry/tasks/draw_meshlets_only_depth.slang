#include "daxa/daxa.inl"
#include "draw_meshlets_only_depth.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsOnlyDepthWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_only_depth_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlets_data_merged->hw_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

[[vk::push_constant]] DrawMeshletsOnlyDepthPush push;
struct MeshPayload {
    u32 index;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void draw_meshlets_only_depth_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void draw_meshlets_only_depth_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {
    const MeshletInstanceData meshlet_instance_data = push.uses.u_meshlets_data_merged.hw_meshlet_data[payload.index];
    const Mesh mesh = push.uses.u_meshes[meshlet_instance_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_instance_data.meshlet_index];

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_instance_data.transform_index].model_matrix);
    const f32mat4x4 pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, transform_matrix);

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
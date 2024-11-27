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
    command.x = write_push.uses.u_meshlet_indices->count;
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

struct Vertex {
    f32vec4 sv_position : SV_Position;
};

struct Primtive {
    nointerpolation [[vk::location(0)]] u32 vis_index;
};

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
    OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {
    const u32 meshlet_index = push.uses.u_meshlet_indices.indices[payload.index];
    const MeshletData meshlet_data = push.uses.u_meshlets_data.meshlets[meshlet_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];
    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);
    u32 triangle_index = local_thread_index.x;

    f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_data.transform_index].model_matrix);
    if(push.uses.u_globals.render_as_observer) {
        pvm = mul(push.uses.u_globals->observer_camera.projection_matrix,
                            mul(push.uses.u_globals->observer_camera.view_matrix, pvm));
    } else {
        pvm = mul(push.uses.u_globals->main_camera.projection_matrix,
                            mul(push.uses.u_globals->main_camera.view_matrix, pvm));
    }
    
    if(triangle_index < meshlet.vertex_count) {
        out_vertices[triangle_index].sv_position = mul(pvm, f32vec4(mesh.vertex_positions[mesh.indirect_vertices[meshlet.indirect_vertex_offset + triangle_index]], 1.0f));
    }

    if(triangle_index < meshlet.triangle_count) {
        out_indices[triangle_index] = u32vec3(
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 0),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 1),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + 2)
        );
        encode_triangle_id(out_primitives[triangle_index].vis_index, meshlet_index, triangle_index);
    }
}

[shader("fragment")]
void draw_meshlets_frag(in Vertex vertex, in Primtive primitive) {
    AtomicMaxU64(tex_rw_u64_table[push.uses.u_visibility_image.index()][u32vec2(vertex.sv_position.xy)], encode_visibility_buffer_payload(primitive.vis_index, vertex.sv_position.z));
}   
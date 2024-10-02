#include "daxa/daxa.inl"
#include "draw_meshlets.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsWriteCommandPush write_push;
[[vk::push_constant]] DrawMeshletsPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_command() {
    DrawIndirectStruct command;
    command.vertex_count = write_push.uses.u_index_buffer->count * 3;
    command.instance_count = 1;
    command.first_vertex = 0;
    command.first_instance = 0;
    *write_push.uses.u_command = command;
    printf("survived meshlets count: %i\n", write_push.uses.u_meshlets_data->count);
}

struct VertexStageOutput {
    u32 vis_index : u32;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const u32 triangle_corner_index = vertex_index % 3;
    u32 inst_meshlet_index;
    u32 triangle_index;
    u32 data = push.uses.u_index_buffer->indices[vertex_index / 3];
    
    decode_triangle_id(data, inst_meshlet_index, triangle_index);

    MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[inst_meshlet_index];
    Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

    VertexStageOutput output;
    const u32 micro_index = get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + triangle_corner_index);
    u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
    vertex_index = min(vertex_index, mesh.vertex_count - 1);

    encode_triangle_id(output.vis_index, inst_meshlet_index, triangle_index);
    
    const f32vec4 vertex_position = f32vec4(mesh.vertex_positions[vertex_index], 1);

    output.sv_position = mul(push.uses.u_globals->camera_projection_matrix,
                        mul(push.uses.u_globals->camera_view_matrix,
                        mul(push.uses.u_transforms[meshlet_data.transform_index].model_matrix,
                        vertex_position)));

    return output;
}

[shader("fragment")]
void fragment_main(u32 vis_index : u32, f32vec4 position: SV_Position) {
    AtomicMaxU64(tex_rw_u64_table[push.uses.u_visibility_image.index()][u32vec2(position.xy)], encode_visibility_buffer_payload(vis_index, position.z));
}   
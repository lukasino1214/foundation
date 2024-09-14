#include "daxa/daxa.inl"
#include "draw_meshlets.inl"
#include "../../../shader_library/vertex_compression.inl"

#if defined(DrawMeshletsWriteCommand_SHADER)
[[vk::push_constant]] DrawMeshletsWriteCommandPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_command() {
    DrawIndirectStruct command;
    command.vertex_count = MAX_TRIANGLES_PER_MESHLET * 3;
    command.instance_count = push.uses.u_meshlets_data->count;
    command.first_vertex = 0;
    command.first_instance = 0;
    *push.uses.u_command = command;
    printf("survived meshlets count: %i\n", push.uses.u_meshlets_data->count);
}
#elif defined(DrawMeshlets_SHADER) || 1
[[vk::push_constant]] DrawMeshletsPush push;

struct VertexStageOutput {
    u32 vis_index : u32;
    f32vec4 sv_position : SV_Position;
};

u32 get_micro_index(u32* micro_indices, u32 index_offset) {
    u32 pack_index = index_offset / 4;
    u32 index_pack = micro_indices[pack_index];
    u32 in_pack_offset = index_offset % 4;
    u32 in_pack_shift = in_pack_offset * 8;
    return (index_pack >> in_pack_shift) & 0xFF;
}

daxa_u32 get_vertex_index(Mesh mesh, Meshlet meshlet, daxa_u32 triangle_index, daxa_u32 offset) {
    daxa_BufferPtr(daxa_u32) micro_index_buffer = mesh.micro_indices;
    const uint micro_index = get_micro_index(micro_index_buffer, meshlet.micro_indices_offset + triangle_index * 3 + offset);
    uint vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
    return min(vertex_index, mesh.vertex_count - 1);
}

void encode_triangle_id(inout u32 id, in u32 meshlet_index, in u32 triangle_index) {
    id = (meshlet_index << 8) | (triangle_index);
}

void decode_triangle_id(in daxa_u32 id, out daxa_u32 meshlet_index, out daxa_u32 triangle_index) {
    meshlet_index = id >> 8;
    triangle_index = id & 0xFF;
}

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const u32 triangle_corner_index = vertex_index % 3;
    const u32 inst_meshlet_index = instance_index;
    const u32 triangle_index = vertex_index / 3;

    MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[inst_meshlet_index];
    Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

    VertexStageOutput output;
    if (triangle_index >= meshlet.triangle_count) {
        output.sv_position = f32vec4(10, 10, 10, 1); // yeet
    } else {
        const u32 micro_index = get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + triangle_corner_index);
        u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
        vertex_index = min(vertex_index, mesh.vertex_count - 1);

        encode_triangle_id(output.vis_index, inst_meshlet_index, triangle_index);
        
        const f32vec4 vertex_position = f32vec4(mesh.vertex_positions[vertex_index], 1);

        output.sv_position = mul(push.uses.u_globals->camera_projection_matrix,
                         mul(push.uses.u_globals->camera_view_matrix,
                         mul(push.uses.u_transforms[meshlet_data.transform_index].model_matrix,
                         vertex_position)));
    }

    return output;
}

[shader("fragment")]
u32 fragment_main(u32 vis_index : u32) : SV_Target {
    return vis_index;
}   
#endif
#include "daxa/daxa.inl"
#include "draw_meshlets_only_depth.inl"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] DrawMeshletsOnlyDepthWriteCommandPush write_push;
[[vk::push_constant]] DrawMeshletsOnlyDepthPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_only_depth_command() {
    DrawIndirectStruct command;
    command.vertex_count = write_push.uses.u_index_buffer->count * 3;
    command.instance_count = 1;
    command.first_vertex = 0;
    command.first_instance = 0;
    *write_push.uses.u_command = command;
    // printf("survived meshlets count: %i\n", push.uses.u_meshlets_data->count);
}

struct VertexStageOutput {
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

    const f32vec4 vertex_position = f32vec4(mesh.vertex_positions[vertex_index], 1);

    output.sv_position = mul(deref(push.uses.u_globals).camera_projection_matrix,
                        mul(deref(push.uses.u_globals).camera_view_matrix,
                        mul(push.uses.u_transforms[meshlet_data.transform_index].model_matrix,
                        vertex_position)));

    return output;
}

f32vec3 hsv_to_rgb(in f32vec3 hsv) {
    const f32vec3 rgb = saturate(abs(fmod(hsv.x * 6.0 + f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * lerp(f32vec3(1.0), rgb, hsv.y);
}

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

[shader("fragment")]
void fragment_main() {
}   
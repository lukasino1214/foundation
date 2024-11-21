#include "daxa/daxa.inl"
#include "draw_meshlets_only_depth.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] HWDrawMeshletsOnlyDepthWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_only_depth_write_command() {
    DrawIndirectStruct command;
    command.vertex_count = MAX_TRIANGLES_PER_MESHLET * 3;
    command.instance_count = write_push.uses.u_meshlet_indices->count;
    command.first_vertex = 0;
    command.first_instance = 0;
    *write_push.uses.u_command = command;
}

[[vk::push_constant]] HWDrawMeshletsOnlyDepthPush push;
struct VertexStageOutput {
    u32 vis_index : u32;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const u32 triangle_corner_index = vertex_index % 3;
    const u32 triangle_index = vertex_index / 3;
    const u32 meshlet_index = push.uses.u_meshlet_indices.indices[instance_index];
    const MeshletData meshlet_data = push.uses.u_meshlets_data.meshlets[meshlet_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

    VertexStageOutput output;
    if(triangle_index >= meshlet.triangle_count || mesh.mesh_buffer.value == 0) {
        output.sv_position = f32vec4(10.0f, 10.0f, 10.0f, 1.0f);
    } else {
        const u32 micro_index = get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + triangle_index * 3 + triangle_corner_index);
        u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
        vertex_index = min(vertex_index, mesh.vertex_count - 1);

        encode_triangle_id(output.vis_index, meshlet_index, triangle_index);
        
        const f32vec4 vertex_position = f32vec4(mesh.vertex_positions[vertex_index], 1);

        output.sv_position = mul(push.uses.u_globals->main_camera.projection_matrix,
                             mul(push.uses.u_globals->main_camera.view_matrix, 
                             mul(push.uses.u_transforms[meshlet_data.transform_index].model_matrix, vertex_position)));
    }

    return output;
}

[shader("fragment")]
void fragment_main(u32 vis_index : u32, f32vec4 position: SV_Position) {}   
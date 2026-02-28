#define DAXA_RAY_TRACING 1
#include "daxa/daxa.inl"
#include "draw_opaque_meshlets.inl"

[[vk::push_constant]] DrawOpaqueMeshletsWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_culled_meshlet_instances_region->count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

[[vk::push_constant]] DrawOpaqueMeshletsPush push;

struct MeshStageOutput {
    f32vec4 sv_position : SV_Position;
};

struct PrimitiveOutput {
    nointerpolation u32 visibility_index;
};

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void mesh_main(
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<MeshStageOutput, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<PrimitiveOutput, MAX_TRIANGLES_PER_MESHLET> out_primitives
) {
    const u32 thread_index = local_thread_index.x;

    const MeshletInstance meshlet_instance = push.uses.u_culled_meshlet_instances_region.meshlet_instances[group_index.x];
    const MeshInstance mesh_instance = push.uses.u_mesh_instances[meshlet_instance.mesh_instance_index];
    const Meshlet meshlet = mesh_instance.meshlets[meshlet_instance.meshlet_index];

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh_instance.mesh_group_index]);
    const f32mat3x3 normal_matrix = f32mat3x3(
        cross(transform_matrix[1].xyz, transform_matrix[2].xyz), 
        cross(transform_matrix[2].xyz, transform_matrix[0].xyz), 
        cross(transform_matrix[0].xyz, transform_matrix[1].xyz)
    );

    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

    if(thread_index < meshlet.vertex_count) {
        const u32 vertex_index = mesh_instance.indirect_vertices[meshlet.indirect_vertex_offset + thread_index];

        MeshStageOutput vertex;
        if(push.uses.u_globals.render_as_observer) {
            vertex.sv_position = mul(push.uses.u_globals->observer_camera.projection_view_matrix, mul(transform_matrix, f32vec4(mesh_instance.positions[vertex_index], 1.0f)));
        } else {
            vertex.sv_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, mul(transform_matrix, f32vec4(mesh_instance.positions[vertex_index], 1.0f)));
        }

        out_vertices[thread_index] = vertex;
    }

    if(thread_index < meshlet.triangle_count) {
        u32vec3 indices = u32vec3(
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0),
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1),
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)
        );

        out_indices[thread_index] = indices;

        PrimitiveOutput primitive;
        encode_triangle_id(primitive.visibility_index, push.uses.u_culled_meshlet_instances_region.offset + group_index.x, thread_index);
        out_primitives[thread_index] = primitive;
    }
}

[shader("fragment")]
u32 fragment_main(
    in PrimitiveOutput primitive
) : SV_Target {
    return primitive.visibility_index;
}   
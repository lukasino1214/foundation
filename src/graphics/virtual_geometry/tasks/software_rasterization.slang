#include "daxa/daxa.inl"
#include "software_rasterization.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/cull_util.slang"
#include "../../../shader_library/software_rasterization.slang"

[[vk::push_constant]] SoftwareRasterizationWriteCommandPush write_push;
[[vk::push_constant]] SoftwareRasterizationPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void software_rasterization_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlet_indices->count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

struct MeshPayload {
    u32 index;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void software_rasterization_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void software_rasterization_mesh(
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

    const f32vec2 scale = f32vec2(0.5, 0.5) * f32vec2(push.uses.u_globals->render_target_size) * f32(SUBPIXEL_SAMPLES);
    const f32vec2 bias = (0.5 * f32vec2(push.uses.u_globals->render_target_size)) * f32(SUBPIXEL_SAMPLES) + 0.5;
    f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_data.transform_index].model_matrix);
    if(push.uses.u_globals.render_as_observer) {
        pvm = mul(push.uses.u_globals->observer_camera.projection_view_matrix, pvm);
    } else {
        pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, pvm);
    }

    VisibilityImageSWFunction sw_function;
    sw_function.visibility_image = push.uses.u_visibility_image.get();

    generic_rasterization<VisibilityImageSWFunction>(
        local_thread_index.x,
        meshlet_index,
        mesh,
        meshlet,
        pvm,
        push.uses.u_globals->render_target_size,
        sw_function
    );
}

[shader("fragment")]
void software_rasterization_frag(in Vertex vertex, in Primtive primitive) {}   
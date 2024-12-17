#include "daxa/daxa.inl"
#include "software_rasterization_only_depth_compute.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/cull_util.slang"
#include "../../../shader_library/software_rasterization.slang"

[[vk::push_constant]] SoftwareRasterizationOnlyDepthComputeWriteCommandPush write_push;
[[vk::push_constant]] SoftwareRasterizationOnlyDepthComputePush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void software_rasterization_write_command() {
    DispatchIndirectStruct command;
#if SW_RASTER_COMPUTE
    command.x = write_push.uses.u_meshlet_indices->count;
    printf("depth: %u\n", write_push.uses.u_meshlet_indices->count);
#else
    command.x = 0;
#endif
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

[shader("compute")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void software_rasterization(
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {

    const u32 meshlet_index = push.uses.u_meshlet_indices.indices[group_index.x];
    const MeshletData meshlet_data = push.uses.u_meshlets_data.meshlets[meshlet_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

    const f32vec2 scale = f32vec2(0.5f, 0.5f) * f32vec2(push.uses.u_globals->render_target_size) * f32(SUBPIXEL_SAMPLES);
    const f32vec2 bias = (0.5 * f32vec2(push.uses.u_globals->render_target_size)) * f32(SUBPIXEL_SAMPLES) + 0.5;
    f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_data.transform_index].model_matrix);
    if(push.uses.u_globals.render_as_observer) {
        pvm = mul(push.uses.u_globals->observer_camera.projection_view_matrix, pvm);
    } else {
        pvm = mul(push.uses.u_globals->main_camera.projection_view_matrix, pvm);
    }

    OnlyDepthSWFunction sw_function;
    sw_function.depth_image = push.uses.u_depth_image.get();

    generic_rasterization<OnlyDepthSWFunction>(
        local_thread_index.x,
        meshlet_index,
        mesh,
        meshlet,
        pvm,
        push.uses.u_globals->render_target_size,
        sw_function
    );
}
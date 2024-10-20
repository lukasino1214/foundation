#include "daxa/daxa.inl"
#include "build_index_buffer.inl"
#include "../../../shader_library/meshlet.slang"

[[vk::push_constant]] HWBuildIndexBufferWriteCommandPush write_push;
[[vk::push_constant]] HWBuildIndexBufferPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void build_index_buffer_write_command() {
    const u32 needed_threads = write_push.uses.u_meshlet_indices->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
    write_push.uses.u_index_buffer->count = 0;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void build_index_buffer(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_meshlet_indices->count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(thread_active) {
        const u32 meshlet_data_index = push.uses.u_meshlet_indices.indices[current_thread_index];
        const MeshletData meshlet_data = push.uses.u_meshlets_data.meshlets[meshlet_data_index];
        const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
        const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];

        uint offset = 0;
        InterlockedAdd(push.uses.u_index_buffer->count, meshlet.triangle_count, offset);

        for(u32 i = 0; i < meshlet.triangle_count; i++) {  
            encode_triangle_id(push.uses.u_index_buffer.indices[offset + i], meshlet_data_index, i);
        }
    }
}
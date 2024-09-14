#include "daxa/daxa.inl"
#include "build_index_buffer.inl"

#if defined(BuildIndexBufferWriteCommand_SHADER)
[[vk::push_constant]] BuildIndexBufferWriteCommandPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void build_index_buffer_write_command() {
    const u32 needed_threads = push.uses.u_meshlets_data->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *push.uses.u_command = command;
    // printf("meshlets to cull: %u\n", needed_threads);
}
#elif defined(BuildIndexBuffer_SHADER) || 1
[[vk::push_constant]] BuildIndexBufferPush push;

void encode_triangle_id(inout u32 id, in u32 meshlet_index, in u32 triangle_index) {
    id = (meshlet_index << 8) | (triangle_index);
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_meshlets_data->count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(current_thread_index == 0) {
        push.uses.u_index_buffer->count = 0;
    }

    if(thread_active) {
        MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[current_thread_index];
        Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
        Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];
        
        uint offset = 0;
        InterlockedAdd(push.uses.u_index_buffer->count, meshlet.triangle_count, offset);

        for(u32 i = 0; i < meshlet.triangle_count; i++) {  
            encode_triangle_id(push.uses.u_index_buffer.data[offset + i], current_thread_index, i);
        }
    }
}
#endif
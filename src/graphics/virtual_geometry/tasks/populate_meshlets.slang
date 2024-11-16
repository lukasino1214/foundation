#include "daxa/daxa.inl"
#include "populate_meshlets.inl"

[[vk::push_constant]] PopulateMeshletsWriteCommandPush write_push;
[[vk::push_constant]] PopulateMeshletsPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void populate_meshlets_write_command() {
    const u32 needed_threads = write_push.uses.u_culled_meshes_data->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
    write_push.uses.u_meshlets_data->count = 0;
}


[shader("compute")]
[numthreads(16, 1, 1)]
void populate_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_culled_meshes_data->count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(thread_active) {
        MeshData mesh_data = push.uses.u_culled_meshes_data->meshes[current_thread_index];
        Mesh mesh = push.uses.u_meshes[mesh_data.global_mesh_index];
        u32 offset;
        InterlockedAdd(push.uses.u_meshlets_data->count, mesh.meshlet_count, offset);

        for(u32 i = 0; i < mesh.meshlet_count; i++) {
            MeshletData meshlet_data;
            meshlet_data.mesh_group_index = mesh_data.mesh_group_index;
            meshlet_data.mesh_index = mesh_data.global_mesh_index;
            meshlet_data.meshlet_index = i;
            meshlet_data.transform_index = mesh_data.transform_index;

            push.uses.u_meshlets_data->meshlets[offset + i] = meshlet_data;
        }
    }
}
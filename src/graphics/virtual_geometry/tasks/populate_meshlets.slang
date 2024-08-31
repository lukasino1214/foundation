#include "daxa/daxa.inl"
#include "populate_meshlets.inl"

#if defined(PopulateMeshletsWriteCommand_SHADER)
[[vk::push_constant]] PopulateMeshletsWriteCommandPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void populate_meshlets_write_command() {
    const u32 needed_threads = push.uses.u_scene_data->entity_count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *push.uses.u_command = command;
    printf("entity count: %u\n", push.uses.u_scene_data->entity_count);
}
#elif defined(PopulateMeshlets_SHADER)
[[vk::push_constant]] PopulateMeshletsPush push;

[shader("compute")]
[numthreads(16, 1, 1)]
void populate_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_scene_data->entity_count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(current_thread_index == 0) {
        push.uses.u_meshlets_data->count = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    if(thread_active) {
        EntityData entity_data = push.uses.u_entities_data[current_thread_index];
        MeshGroup mesh_group = push.uses.u_mesh_groups[entity_data.mesh_group_index];

        for(u32 mesh_index = 0; mesh_index < mesh_group.count; mesh_index++) {
            u32 global_mesh_index = mesh_group.mesh_indices[mesh_index];
            Mesh mesh = push.uses.u_meshes[global_mesh_index];
            u32 offset;
            InterlockedAdd(push.uses.u_meshlets_data->count, mesh.meshlet_count, offset);

            for(daxa_u32 i = 0; i < mesh.meshlet_count; i++) {
                MeshletData meshlet_data;
                meshlet_data.mesh_group_index = entity_data.mesh_group_index;
                meshlet_data.mesh_index = global_mesh_index;
                meshlet_data.meshlet_index = i;
                meshlet_data.transform_index = entity_data.transform_index;

                push.uses.u_meshlets_data->meshlets[offset + i] = meshlet_data;
            }
        }
    }
}
#endif


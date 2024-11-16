#include "daxa/daxa.inl"
#include "cull_meshes.inl"
#include "../../../shader_library/cull_util.slang"

[[vk::push_constant]] CullMeshesWriteCommandPush write_push;
[[vk::push_constant]] CullMeshesPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_meshes_write_command() {
    const u32 needed_threads = write_push.uses.u_scene_data->entity_count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
    write_push.uses.u_culled_meshes_data->count = 0;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshes(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_scene_data->entity_count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(thread_active) {
        EntityData entity_data = push.uses.u_entities_data[current_thread_index];
        MeshGroup mesh_group = push.uses.u_mesh_groups[entity_data.mesh_group_index];

         for(u32 local_mesh_index = 0; local_mesh_index < mesh_group.count; local_mesh_index++) {
            const u32 global_mesh_index = mesh_group.mesh_indices[local_mesh_index];
            const Mesh mesh = push.uses.u_meshes[global_mesh_index];

            MeshData mesh_data;
            mesh_data.mesh_group_index = entity_data.mesh_group_index;
            mesh_data.local_mesh_index = local_mesh_index;
            mesh_data.global_mesh_index = global_mesh_index;
            mesh_data.transform_index = entity_data.transform_index;

            u32 offset = 0;
            InterlockedAdd(push.uses.u_culled_meshes_data->count, 1, offset);
            push.uses.u_culled_meshes_data->meshes[offset] = mesh_data;
        }
    }
}
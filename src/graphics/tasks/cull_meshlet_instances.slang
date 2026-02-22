#include "daxa/daxa.inl"
#include "cull_meshlet_instances.inl"
#include "../../shader_library/work_expansion.slang"

[[vk::push_constant]] CullMeshletInstancesWriteCommandPush write_push;
[[vk::push_constant]] CullMeshletInstancesPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_meshlet_instances_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_prefix_sum_work_expansion_mesh->workgroup_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;

    write_push.uses.u_culled_meshlet_instances_data->count = 0;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshlet_instances(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 current_thread_index = thread_index.x;

    DstItemInfo dst_item_info;
    if(!prefix_sum_expansion_get_workitem(push.uses.u_prefix_sum_work_expansion_mesh, current_thread_index, dst_item_info)) {
        return;
    }

    u32 meshlet_index = dst_item_info.in_expansion_index;
    const u32 mesh_instance_index = push.uses.u_culled_mesh_instances_data->mesh_instances[dst_item_info.src_item_index];
    const MeshInstance mesh_instance = push.uses.u_mesh_instances[mesh_instance_index];

    MeshletInstance meshlet_instance;
    meshlet_instance.mesh_instance_index = mesh_instance_index;
    meshlet_instance.meshlet_index = meshlet_index;

    u32 i = 0;
    InterlockedAdd(push.uses.u_culled_meshlet_instances_data->count, 1, i);
    push.uses.u_culled_meshlet_instances_data.meshlet_instances[i] = meshlet_instance;
}
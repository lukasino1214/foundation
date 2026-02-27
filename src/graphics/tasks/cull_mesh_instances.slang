#include "daxa/daxa.inl"
#include "cull_mesh_instances.inl"
#include "../../shader_library/cull_util.slang"
#include "../../shader_library/work_expansion.slang"

[[vk::push_constant]] CullMeshInstancesWriteCommandPush write_push;
[[vk::push_constant]] CullMeshInstancesPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_mesh_instances_write_command() {
    const u32 needed_threads = *write_push.uses.u_mesh_group_instance_count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;

    write_push.uses.u_culled_mesh_instances_data->count = 0;

    write_push.uses.u_prefix_sum_work_expansion_mesh->merged_expansion_count_thread_count = 0;
    write_push.uses.u_prefix_sum_work_expansion_mesh->workgroup_count = 0;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_mesh_instances(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = *push.uses.u_mesh_group_instance_count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(thread_active) {
        MeshGroupInstance mesh_group_instance = push.uses.u_mesh_group_instances[current_thread_index];

        for(u32 local_mesh_index = 0; local_mesh_index < mesh_group_instance.count; local_mesh_index++) {
            const u32 mesh_instance_index = push.uses.u_mesh_instance_indices[mesh_group_instance.mesh_indices_offset + local_mesh_index];
            const MeshInstance mesh = push.uses.u_mesh_instances[mesh_instance_index];

            if(mesh.mesh_buffer.value == 0) { continue; }

            const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[current_thread_index]);
            const AABB aabb = calculate_worldspace_aabb_from_modelspace(transform_matrix, mesh.mesh_aabb);
            if(!is_worldspace_aabb_frusum_visible(push.uses.u_globals.main_camera, aabb)) { continue; }

            AABB mesh_aabb = mesh.mesh_aabb;
            NdcAABB ndc_aabb = calculate_ndc_aabb(push.uses.u_globals.main_camera, transform_matrix, mesh_aabb);
            if(is_ndc_aabb_hiz_depth_occluded(
                push.uses.u_globals.cull_data,
                push.uses.u_globals.main_camera,
                ndc_aabb,
                push.uses.u_hiz
            )) { 
                continue; 
            }

            // ShaderDebugAABBDraw draw;
            // draw.color = f32vec3(1, 0, 0);
            // draw.position = aabb.center;
            // draw.extent = aabb.extent;

            // push.uses.u_globals.debug_draw(draw);

            u32 offset = 0;
            InterlockedAdd(push.uses.u_culled_mesh_instances_data->count, 1, offset);
            push.uses.u_culled_mesh_instances_data->mesh_instances[offset] = mesh_instance_index;

            prefix_sum_expansion_add_workitems(push.uses.u_prefix_sum_work_expansion_mesh, offset, mesh.meshlet_count, 16);
        }
    }
}
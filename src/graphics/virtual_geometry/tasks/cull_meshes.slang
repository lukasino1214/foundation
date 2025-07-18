#include "daxa/daxa.inl"
#include "cull_meshes.inl"
#include "../../../shader_library/cull_util.slang"
#include "../../../shader_library/work_expansion.slang"

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

    write_push.uses.u_opaque_prefix_sum_work_expansion_mesh->merged_expansion_count_thread_count = 0;
    write_push.uses.u_opaque_prefix_sum_work_expansion_mesh->workgroup_count = 0;

    write_push.uses.u_masked_prefix_sum_work_expansion_mesh->merged_expansion_count_thread_count = 0;
    write_push.uses.u_masked_prefix_sum_work_expansion_mesh->workgroup_count = 0;

    write_push.uses.u_transparent_prefix_sum_work_expansion_mesh->merged_expansion_count_thread_count = 0;
    write_push.uses.u_transparent_prefix_sum_work_expansion_mesh->workgroup_count = 0;
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

            const f32mat4x4 model_matrix = mat_4x3_to_4x4(push.uses.u_transforms[entity_data.transform_index].model_matrix);
            const AABB worldspace_aabb = calculate_worldspace_aabb_from_modelspace(model_matrix, mesh.aabb);

            if(!is_worldspace_aabb_frusum_visible(push.uses.u_globals->main_camera, worldspace_aabb)) {
                continue;
            }

            const f32mat4x4 projection_view_matrix = push.uses.u_globals->main_camera.projection_view_matrix;
            const NdcAABB ndc_aabb = calculate_ndc_aabb_from_worldspace(model_matrix, projection_view_matrix, mesh.aabb);

            if(!is_ndc_aabb_hiz_depth_visible(ndc_aabb, push.uses.u_globals->next_lower_po2_render_target_size, push.uses.u_hiz)) {
                continue;
            }

            InterlockedOr(push.uses.u_readback_mesh[global_mesh_index / 32u], 1u << (global_mesh_index % 32u));

            if(mesh.mesh_buffer.value == 0) { continue; }
            MeshData mesh_data;
            mesh_data.global_mesh_index = global_mesh_index;
            mesh_data.transform_index = entity_data.transform_index;

            u32 offset = 0;
            InterlockedAdd(push.uses.u_culled_meshes_data->count, 1, offset);
            push.uses.u_culled_meshes_data->meshes[offset] = mesh_data;

            PrefixSumWorkExpansion* prefix_sum_work_expansion = nullptr;
            if(push.uses.u_materials[mesh.material_index].alpha_mode == 0) { prefix_sum_work_expansion = push.uses.u_opaque_prefix_sum_work_expansion_mesh; }
            else if(push.uses.u_materials[mesh.material_index].alpha_mode == 1) { prefix_sum_work_expansion = push.uses.u_masked_prefix_sum_work_expansion_mesh; }
            else if(push.uses.u_materials[mesh.material_index].alpha_mode == 2) { prefix_sum_work_expansion = push.uses.u_transparent_prefix_sum_work_expansion_mesh; }
            prefix_sum_expansion_add_workitems(prefix_sum_work_expansion, offset, mesh.meshlet_count, 16);
        }
    }
}
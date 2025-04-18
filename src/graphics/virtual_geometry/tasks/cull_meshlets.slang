#include "daxa/daxa.inl"
#include "cull_meshlets.inl"
#include "../../../shader_library/cull_util.slang"
#include "../../../shader_library/work_expansion.slang"

[[vk::push_constant]] CullMeshletsWriteCommandPush write_push;
[[vk::push_constant]] CullMeshletsPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_meshlets_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_prefix_sum_work_expansion_mesh->workgroup_count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
    write_push.uses.u_meshlets_data_merged->hw_count = 0;
    write_push.uses.u_meshlets_data_merged->sw_count = 0;
}

bool lod_error_is_imperceptible(BoundingSphere lod_sphere, f32 simplification_error, const f32mat4x4 transform_matrix, const f32mat4x4 projection_matrix) {
    let sphere_world_space = mul(transform_matrix, f32vec4(lod_sphere.center, 1.0)).xyz;
    let radius_world_space = lod_sphere.radius;
    let error_world_space = simplification_error;

    var projected_error = error_world_space;

    let distance_to_closest_point_on_sphere = distance(sphere_world_space, push.uses.u_globals->main_camera.position) - radius_world_space;
    let distance_to_closest_point_on_sphere_clamped_to_znear = max(distance_to_closest_point_on_sphere, projection_matrix[2][3]);
    projected_error /= distance_to_closest_point_on_sphere_clamped_to_znear;

    projected_error *= -projection_matrix[1][1] * 0.5;
    projected_error *= push.uses.u_globals->render_target_size.y;

    return projected_error < 1.0;
}

#define VIEWPORT 0

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 current_thread_index = thread_index.x;

    DstItemInfo dst_item_info;
    if(!prefix_sum_expansion_get_workitem(push.uses.u_prefix_sum_work_expansion_mesh, current_thread_index, dst_item_info)) {
        return;
    }

    u32 meshlet_index = dst_item_info.in_expansion_index;
    const MeshData mesh_data = push.uses.u_culled_meshes_data->meshes[dst_item_info.src_item_index];
    const Mesh mesh = push.uses.u_meshes[mesh_data.global_mesh_index];
    const AABB aabb = mesh.meshlet_aabbs[dst_item_info.in_expansion_index];
    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh_data.transform_index].model_matrix);
    const u32vec2 viewport_size = push.uses.u_globals->render_target_size.xy;

    const f32mat4x4 mvp = mul(push.uses.u_globals->main_camera.projection_view_matrix, transform_matrix);
    const f32mat4x4 projection_matrix = push.uses.u_globals->main_camera.projection_matrix;
    
    const AABB worldspace_aabb = calculate_worldspace_aabb_from_modelspace(transform_matrix, aabb);
    if(!is_worldspace_aabb_frusum_visible(push.uses.u_globals->main_camera, worldspace_aabb)) {
        return;
    }
    
    NdcAABB ndc_aabb;
    bool behind_near_plane = false;
    bool clip = false;
    const f32vec3 model_corner_position = aabb.center + aabb.extent * f32vec3(-1.0f, -1.0f, -1.0f);
    const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position,1));
    const f32vec3 ndc_corner_position = clipspace_corner_position.xyz * rcp(clipspace_corner_position.w);
    ndc_aabb.ndc_min = ndc_corner_position.xyz;
    ndc_aabb.ndc_max = ndc_corner_position.xyz;
    behind_near_plane = behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
    
    for (u32 i = 1; i < 8; i++) {
        const f32vec3 corner = f32vec3(i & 0x1, i & 0x2, i & 0x4) * f32vec3(1, 0.5f, 0.25f);
        const f32vec3 model_corner_position = aabb.center + aabb.extent * corner;
        const f32vec4 clipspace_corner_position = mul(mvp, f32vec4(model_corner_position, 1.0f));
        const f32vec3 ndc_corner_position = clipspace_corner_position.xyz * rcp(clipspace_corner_position.w);
        ndc_aabb.ndc_min = min(ndc_aabb.ndc_min, ndc_corner_position.xyz);
        ndc_aabb.ndc_max = max(ndc_aabb.ndc_max, ndc_corner_position.xyz);
        behind_near_plane = behind_near_plane || (clipspace_corner_position.z > clipspace_corner_position.w);
        clip = clip || ndc_corner_position.x <= -1.0 || ndc_corner_position.x >= 1.0 || ndc_corner_position.y <= -1.0 || ndc_corner_position.y >= 1.0;
    }
    
    ndc_aabb.ndc_min.x = max(ndc_aabb.ndc_min.x, -1.0f);
    ndc_aabb.ndc_min.y = max(ndc_aabb.ndc_min.y, -1.0f);
    ndc_aabb.ndc_max.x = min(ndc_aabb.ndc_max.x,  1.0f);
    ndc_aabb.ndc_max.y = min(ndc_aabb.ndc_max.y,  1.0f);
    if(behind_near_plane) { ndc_aabb.ndc_min = ndc_aabb.ndc_max = f32vec3(0.0f, 0.0f, INVALID_NDC_AABB_Z); }
    
    if(!is_ndc_aabb_hiz_depth_visible(ndc_aabb, push.uses.u_globals->next_lower_po2_render_target_size, push.uses.u_hiz)) {
        return;
    }
    
    const MeshletBoundingSpheres bounding_spheres = mesh.bounding_spheres[meshlet_index];
    const MeshletSimplificationError simplification_errors = mesh.simplification_errors[meshlet_index];
    let lod_is_ok = lod_error_is_imperceptible(bounding_spheres.lod_group_sphere, simplification_errors.group_error, transform_matrix, projection_matrix);
    let parent_lod_is_ok = lod_error_is_imperceptible(bounding_spheres.lod_parent_group_sphere, simplification_errors.parent_group_error, transform_matrix, projection_matrix);
    if (!lod_is_ok || parent_lod_is_ok) { 
        return; 
    }

    i32vec2 min_viewport = i32vec2((ndc_aabb.ndc_min.xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_size));
    i32vec2 max_viewport = i32vec2((ndc_aabb.ndc_max.xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_size));
    min_viewport = max(min_viewport, i32vec2(0));
    max_viewport = min(max_viewport, viewport_size - 1);

    const bool is_sw_candidate =
        !clip &&
        !behind_near_plane && 
        abs(max_viewport.x - min_viewport.x) < 32 &&
        abs(max_viewport.y - min_viewport.y) < 32;

    MeshletInstanceData meshlet_instance_data;
    meshlet_instance_data.mesh_index = mesh_data.global_mesh_index;
    meshlet_instance_data.meshlet_index = meshlet_index;
    meshlet_instance_data.transform_index = mesh_data.transform_index;

    if(is_sw_candidate && push.uses.u_globals->render_as_observer == 0) {
        u32 i = 0;
        InterlockedAdd(push.uses.u_meshlets_data_merged->sw_count, 1, i);
        push.uses.u_meshlets_data_merged.sw_meshlet_data[i] = meshlet_instance_data;
    } else {
        u32 i = 0;
        InterlockedAdd(push.uses.u_meshlets_data_merged->hw_count, 1, i);
        push.uses.u_meshlets_data_merged.hw_meshlet_data[i] = meshlet_instance_data;
    }
}
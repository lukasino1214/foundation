#include "daxa/daxa.inl"
#include "cull_meshlets.inl"
#include "../../../shader_library/cull_util.slang"

[[vk::push_constant]] CullMeshletsWriteCommandPush write_push;
[[vk::push_constant]] CullMeshletsPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_meshlets_write_command() {
    const u32 needed_threads = write_push.uses.u_meshlets_data->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
    write_push.uses.u_hw_culled_meshlet_indices->count = 0;
    write_push.uses.u_sw_culled_meshlet_indices->count = 0;
}

bool is_meshlet_visible(AABB aabb, f32mat4x4 model) {
    const AABB worldspace_aabb = calculate_worldspace_aabb_from_modelspace(model, aabb);
    if(!is_worldspace_aabb_frusum_visible(push.uses.u_globals->main_camera, worldspace_aabb)) {
        return false;
    }

    const NdcAABB ndc_aabb = calculate_ndc_aabb_from_worldspace(model, push.uses.u_globals->main_camera.projection_view_matrix, aabb);
    return !is_ndc_aabb_hiz_depth_visible(ndc_aabb, push.uses.u_globals->next_lower_po2_render_target_size, push.uses.u_hiz);
}

void write_meshlet(MeshletIndices* meshlets_indices, in u32 index) {
    u32 i = 0;
    InterlockedAdd(meshlets_indices->count, 1, i);
    meshlets_indices.indices[i] = index;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_meshlets_data->count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(thread_active) {
        const MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[current_thread_index];
        const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
        const AABB aabb = mesh.meshlet_aabbs[meshlet_data.meshlet_index];
        const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_data.transform_index].model_matrix);

        if(is_meshlet_visible(aabb, transform_matrix)) {
            const u32 viewport_width = push.uses.u_globals->render_target_size.x;
            const u32 viewport_height = push.uses.u_globals->render_target_size.y;

            const f32vec3 aabb_min = aabb.center - aabb.extent;
            const f32vec3 aabb_max = aabb.center + aabb.extent;
            const f32vec3 aabb_size = aabb_max - aabb_min + 1e-5;
            const f32vec3[8] corners = f32vec3[8](
                aabb_min,
                aabb_min + f32vec3(aabb_size.x, 0.0, 0.0),
                aabb_min + f32vec3(0.0, aabb_size.y, 0.0),
                aabb_min + f32vec3(0.0, 0.0, aabb_size.z),
                aabb_min + f32vec3(aabb_size.xy, 0.0),
                aabb_min + f32vec3(0.0, aabb_size.yz),
                aabb_min + f32vec3(aabb_size.x, 0.0, aabb_size.z),
                aabb_min + aabb_size);
            f32vec2 min_xy = f32vec2(3.402823466e+38);
            f32vec2 max_xy = f32vec2(-3.402823466e+38);
            f32 min_z = 3.402823466e+38;
            bool clip = false;
            for (u32 i = 0; i < 8; ++i) {
                const f32vec4 worldspace_corner_position = mul(transform_matrix, f32vec4(corners[i], 1));
                const f32vec4 clipspace_corner_position = mul(push.uses.u_globals->main_camera.projection_view_matrix, worldspace_corner_position);
                const f32vec3 ndc = clipspace_corner_position.xyz / clipspace_corner_position.w;
                min_z = min(min_z, ndc.z);
                if (ndc.x <= -1.0 || ndc.x >= 1.0 || ndc.y <= -1.0 || ndc.y >= 1.0 || min_z <= 0.0) {
                    clip = true;
                    break;
                }
                min_xy = min(min_xy, ndc.xy);
                max_xy = max(max_xy, ndc.xy);
            }
            i32vec2 min_viewport = i32vec2((min_xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_width, viewport_height));
            i32vec2 max_viewport = i32vec2((max_xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_width, viewport_height));
            min_viewport = max(min_viewport, i32vec2(0));
            max_viewport = min(max_viewport, i32vec2(viewport_width, viewport_height) - 1);

            const bool is_sw_candidate =
                !clip &&
                abs(max_viewport.x - min_viewport.x) < 32 &&
                abs(max_viewport.y - min_viewport.y) < 32;

            if(is_sw_candidate && push.uses.u_globals->render_as_observer == 0) {
                write_meshlet(push.uses.u_sw_culled_meshlet_indices, current_thread_index);
            } else {
                write_meshlet(push.uses.u_hw_culled_meshlet_indices, current_thread_index);
            }
        }
    }
}
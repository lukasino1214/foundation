#include "daxa/daxa.inl"
#include "skin_animated_meshes.inl"
#include "../../../shader_library/work_expansion.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/vertex_compression.inl"

[[vk::push_constant]] AddAnimatedMeshesToPrefixSumWriteCommandPush add_write_push;
[shader("compute")]
[numthreads(1, 1, 1)]
void add_animated_meshes_to_prefix_sum_write_command() {
    const u32 needed_threads = *add_write_push.uses.u_animated_mesh_count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *add_write_push.uses.u_command = command;
}

[[vk::push_constant]] AddAnimatedMeshesToPrefixSumPush add_push;
[shader("compute")]
[numthreads(16, 1, 1)]
void add_animated_meshes_to_prefix_sum(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 needed_threads = *add_push.uses.u_animated_mesh_count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < needed_threads;

    if(thread_active) {
        AnimatedMesh animated_mesh = add_push.uses.u_animated_meshes[current_thread_index];
        prefix_sum_expansion_add_workitems(add_push.uses.u_animated_mesh_vertices_prefix_sum_work_expansion, current_thread_index, animated_mesh.vertex_count, 16);
        prefix_sum_expansion_add_workitems(add_push.uses.u_animated_mesh_meshlets_prefix_sum_work_expansion, current_thread_index, animated_mesh.meshlet_count, 1);
    }
}

[[vk::push_constant]] SkinAnimatedMeshesWriteCommandPush skin_write_push;
[shader("compute")]
[numthreads(1, 1, 1)]
void skin_animated_meshes_write_command() {
    DispatchIndirectStruct command;
    command.x = skin_write_push.uses.u_animated_mesh_vertices_prefix_sum_work_expansion->workgroup_count;
    command.y = 1;
    command.z = 1;
    *skin_write_push.uses.u_command = command;
}

[[vk::push_constant]] SkinAnimatedMeshesPush skin_push;
[shader("compute")]
[numthreads(16, 1, 1)]
void skin_animated_meshes(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 current_thread_index = thread_index.x;

    DstItemInfo dst_item_info;
    if(!prefix_sum_expansion_get_workitem(skin_push.uses.u_animated_mesh_vertices_prefix_sum_work_expansion, current_thread_index, dst_item_info)) {
        return;
    }

    u32 animated_mesh_index = dst_item_info.src_item_index;
    u32 vertex_index = dst_item_info.in_expansion_index;

    AnimatedMesh animated_mesh = skin_push.uses.u_animated_meshes[animated_mesh_index];

    if(vertex_index < animated_mesh.vertex_count) {
        f32vec3 position = animated_mesh.original_vertex_positions[vertex_index];
        
        for(u32 i = 0; i < animated_mesh.morph_target_position_count; i++) {
            position += animated_mesh.calculated_weights[i] * animated_mesh.morph_target_positions[animated_mesh.vertex_count * i + vertex_index];;
        }

        animated_mesh.vertex_positions[vertex_index] = position;

        f32vec3 normal = decode_normal(animated_mesh.original_vertex_normals[vertex_index]);

        for(u32 i = 0; i < animated_mesh.morph_target_normal_count; i++) {
            normal += animated_mesh.calculated_weights[i] * decode_normal(animated_mesh.morph_target_normals[animated_mesh.vertex_count * i + vertex_index]);
        }

        animated_mesh.vertex_normals[vertex_index] = encode_normal(normalize(normal));

        f32vec2 uv = decode_uv(animated_mesh.original_vertex_uvs[vertex_index]);

        for(u32 i = 0; i < animated_mesh.morph_target_uv_count; i++) {
            uv += animated_mesh.calculated_weights[i] * decode_uv(animated_mesh.morph_target_uvs[animated_mesh.vertex_count * i + vertex_index]);
        }

        animated_mesh.vertex_uvs[vertex_index] = encode_uv(uv);
    }
}

[[vk::push_constant]] CalculateBoundsAnimatedMeshesWriteCommandPush calculate_bounds_write_push;
[shader("compute")]
[numthreads(1, 1, 1)]
void calculate_bounds_animated_meshes_write_command() {
    DispatchIndirectStruct command;
    command.x = calculate_bounds_write_push.uses.u_animated_mesh_meshlets_prefix_sum_work_expansion->workgroup_count;
    command.y = 1;
    command.z = 1;
    *calculate_bounds_write_push.uses.u_command = command;
}

groupshared u32 lds_max_x;
groupshared u32 lds_max_y;
groupshared u32 lds_max_z;

groupshared u32 lds_min_x;
groupshared u32 lds_min_y;
groupshared u32 lds_min_z;

u32 order_preserving_float_map(f32 value) {
    u32 uvalue = asuint(value);
    u32 mask = -i32(uvalue >> 31) | 0x80000000;
    return uvalue ^ mask;
}

f32 inverse_order_preserving_float_map(u32 value) {
    u32 mask = ((value >> 31) - 1) | 0x80000000;
    return asfloat(value ^ mask);
}

[[vk::push_constant]] CalculateBoundsAnimatedMeshesPush calculate_bounds_push;
[shader("compute")]
[numthreads(MAX_VERTICES_PER_MESHLET, 1, 1)]
void calculate_bounds_animated_meshes(u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 group_id : SV_GroupID) {
    const u32 current_global_thread_index = global_thread_index.x;
    const u32 current_local_thread_index = local_thread_index.x;

    if(current_local_thread_index == 0) {
        lds_max_x = 0;
        lds_max_y = 0;
        lds_max_z = 0;

        lds_min_x = 0xffffffffu;
        lds_min_y = 0xffffffffu;
        lds_min_z = 0xffffffffu;
    }

    GroupMemoryBarrierWithGroupSync();

    DstItemInfo dst_item_info;
    if(!prefix_sum_expansion_get_workitem(calculate_bounds_push.uses.u_animated_mesh_meshlets_prefix_sum_work_expansion, group_id.x, dst_item_info)) {
        return;
    }

    u32 animated_mesh_index = dst_item_info.src_item_index;
    u32 meshlet_index = dst_item_info.in_expansion_index;

    AnimatedMesh animated_mesh = calculate_bounds_push.uses.u_animated_meshes[animated_mesh_index];
    Meshlet meshlet = animated_mesh.meshlets[meshlet_index];

    if(current_local_thread_index < meshlet.vertex_count) {
        f32vec3 position = animated_mesh.vertex_positions[animated_mesh.indirect_vertices[meshlet.indirect_vertex_offset + current_local_thread_index]];

        u32 x = order_preserving_float_map(position.x);
        AtomicMaxU32(lds_max_x, x);
        AtomicMinU32(lds_min_x, x);

        u32 y = order_preserving_float_map(position.y);
        AtomicMaxU32(lds_max_y, y);
        AtomicMinU32(lds_min_y, y);

        u32 z = order_preserving_float_map(position.z);
        AtomicMaxU32(lds_max_z, z);
        AtomicMinU32(lds_min_z, z);
    }

    GroupMemoryBarrierWithGroupSync();

    if(current_local_thread_index == 0) {
        f32vec3 min = f32vec3(
            inverse_order_preserving_float_map(lds_min_x), 
            inverse_order_preserving_float_map(lds_min_y), 
            inverse_order_preserving_float_map(lds_min_z)
        );

        f32vec3 max = f32vec3(
            inverse_order_preserving_float_map(lds_max_x), 
            inverse_order_preserving_float_map(lds_max_y), 
            inverse_order_preserving_float_map(lds_max_z)
        );

        AABB aabb;
        aabb.center = (min + max) * 0.5f;
        aabb.extent = (max - min) * 0.5f;

        animated_mesh.meshlet_aabbs[meshlet_index] = aabb;

        MeshletBoundingSpheres sphere;
        sphere.culling_sphere.center = aabb.center;
        sphere.culling_sphere.radius = distance(aabb.center, aabb.extent);

        animated_mesh.bounding_spheres[meshlet_index] = sphere;
    }
}
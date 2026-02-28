#define DAXA_RAY_TRACING 1
#include "daxa/daxa.inl"
#include "draw_masked_meshlets.inl"

[[vk::push_constant]] DrawMaskedMeshletsWriteCommandPush write_push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_culled_meshlet_instances_region->count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

[[vk::push_constant]] DrawMaskedMeshletsPush push;

struct MeshStageOutput {
    f32vec4 sv_position : SV_Position;
    f32vec2 uv;
};

struct PrimitiveOutput {
    nointerpolation [[vk::location(0)]] u32 visibility_index;
};

struct MeshPrimitiveOutput {
    bool cull_primitive : SV_CullPrimitive;
    PrimitiveOutput data; 
};

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MESH_SHADER_WORKGROUP_X, 1, 1)]
void mesh_main(
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<MeshStageOutput, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<MeshPrimitiveOutput, MAX_TRIANGLES_PER_MESHLET> out_primitives
) {
    const u32 thread_index = local_thread_index.x;

    const MeshletInstance meshlet_instance = push.uses.u_culled_meshlet_instances_region.meshlet_instances[group_index.x];
    const MeshInstance mesh_instance = push.uses.u_mesh_instances[meshlet_instance.mesh_instance_index];
    const Meshlet meshlet = mesh_instance.meshlets[meshlet_instance.meshlet_index];

    const f32mat4x4 transform_matrix = mat_4x3_to_4x4(push.uses.u_transforms[mesh_instance.mesh_group_index]);
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->main_camera.projection_view_matrix;

    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

    const u32 vertex_loops = (MAX_VERTICES_PER_MESHLET + MESH_SHADER_WORKGROUP_X - 1) / MESH_SHADER_WORKGROUP_X;

    f32vec4 local_clip_vertices[2];

    [[unroll]]
    for(u32 loop = 0; loop < vertex_loops; loop++) {
        u32 index = thread_index + loop * MESH_SHADER_WORKGROUP_X;
        index = min(index, meshlet.vertex_count - 1);

        {
            const u32 vertex_index = mesh_instance.indirect_vertices[meshlet.indirect_vertex_offset + index];
            MeshStageOutput vertex;
            local_clip_vertices[loop] = mul(projection_view_matrix, mul(transform_matrix, f32vec4(mesh_instance.positions[vertex_index], 1.0f)));
            vertex.sv_position = local_clip_vertices[loop];
            vertex.uv = mesh_instance.uvs[vertex_index];
            out_vertices[index] = vertex;
        }
    }

    const u32 triangle_loops = (MAX_TRIANGLES_PER_MESHLET + MESH_SHADER_WORKGROUP_X - 1) / MESH_SHADER_WORKGROUP_X;

    [[unroll]]
    for(u32 loop = 0; loop < triangle_loops; loop++) {
        u32 index = thread_index + loop * MESH_SHADER_WORKGROUP_X;
        index = min(index, meshlet.triangle_count - 1);

        const u32vec3 indices = u32vec3(
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + index * 3 + 0),
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + index * 3 + 1),
            get_micro_index(mesh_instance.micro_indices, meshlet.micro_indices_offset + index * 3 + 2)
        );

        const f32vec4[3] triangle_clip_positions = f32vec4[3](
            shuffle_arr2(local_clip_vertices, indices[0]),
            shuffle_arr2(local_clip_vertices, indices[1]),
            shuffle_arr2(local_clip_vertices, indices[2])
        );

        bool cull_primitive = false;
        if(!cull_primitive) {
            const f32vec3[3] triangle_ndc_positions = f32vec3[3](
                triangle_clip_positions[0].xyz / (triangle_clip_positions[0].w),
                triangle_clip_positions[1].xyz / (triangle_clip_positions[1].w),
                triangle_clip_positions[2].xyz / (triangle_clip_positions[2].w)
            );

            const f32vec2 ndc_min = min(min(triangle_ndc_positions[0].xy, triangle_ndc_positions[1].xy), triangle_ndc_positions[2].xy);
            const f32vec2 ndc_max = max(max(triangle_ndc_positions[0].xy, triangle_ndc_positions[1].xy), triangle_ndc_positions[2].xy);
            cull_primitive = is_triangle_invisible_micro_triangle( ndc_min, ndc_max, f32vec2(push.uses.u_globals.render_target_size));;
        }

        {
            MeshPrimitiveOutput primitive;
            primitive.cull_primitive = cull_primitive;
            encode_triangle_id(primitive.data.visibility_index, group_index.x, index);
            out_primitives[index] = primitive;
        }

        if(!cull_primitive) {
            out_indices[index] = indices;
        }
    }
}

[shader("fragment")]
u32 fragment_main(in MeshStageOutput input, in PrimitiveOutput primitive) : SV_Target {
    u32 meshlet_instance_index;
    u32 triangle_index;
    decode_triangle_id(primitive.visibility_index, meshlet_instance_index, triangle_index);

    const MeshletInstance meshlet_instance = push.uses.u_culled_meshlet_instances_region.meshlet_instances[meshlet_instance_index];
    const MeshInstance mesh_instance = push.uses.u_mesh_instances[meshlet_instance.mesh_instance_index];

    if(mesh_instance.material_index != INVALID_ID) {
        const Material material = push.uses.u_materials[mesh_instance.material_index];

        f32 alpha = material.albedo_factor.a;
        if(!material.albedo_image_id.is_empty()) {
            alpha = Texture2D<f32vec4>::get(material.albedo_image_id).Sample(SamplerState::get(material.albedo_sampler_id), input.uv).a;
            f32 lod = Texture2D<f32vec4>::get(material.albedo_image_id).CalculateLevelOfDetail(SamplerState::get(material.albedo_sampler_id), input.uv);
            alpha *= 1.0f + max(0.0f, lod) * 0.25f;
            alpha = saturate(alpha);
        }

        if(alpha < 0.5f) { discard; }
    }

    u32 visibility_index = 0;
    encode_triangle_id(visibility_index, push.uses.u_culled_meshlet_instances_region.offset + meshlet_instance_index, triangle_index);
    return visibility_index;
}   

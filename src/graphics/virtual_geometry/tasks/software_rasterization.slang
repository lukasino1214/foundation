#include "daxa/daxa.inl"
#include "software_rasterization.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/cull_util.slang"
#include "../../../shader_library/software_rasterization.slang"

[[vk::push_constant]] SoftwareRasterizationWriteCommandPush write_push;
[[vk::push_constant]] SoftwareRasterizationPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void software_rasterization_write_command() {
    DispatchIndirectStruct command;
    command.x = write_push.uses.u_meshlet_indices->count;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

struct MeshPayload {
    u32 index;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void software_rasterization_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    MeshPayload payload;
    payload.index = global_thread_index.x;
    DispatchMesh(1, 1, 1, payload);
}

struct Vertex {
    f32vec4 sv_position : SV_Position;
};

struct Primtive {
    nointerpolation [[vk::location(0)]] u32 vis_index;
    bool cull_primitive : SV_CullPrimitive;
};

struct VisibilityImageSWFunction : SWRasterFunction {
    u32 payload;
    RWTexture2D<u64> visibility_image;

    void write(u32vec2 position, f32 depth) {
        AtomicMaxU64(visibility_image[position], encode_visibility_buffer_payload(payload, depth));
    }
};

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(MAX_TRIANGLES_PER_MESHLET, 1, 1)]
void software_rasterization_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {

    const u32 meshlet_index = push.uses.u_meshlet_indices.indices[payload.index];
    const MeshletData meshlet_data = push.uses.u_meshlets_data.meshlets[meshlet_index];
    const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
    const Meshlet meshlet = mesh.meshlets[meshlet_data.meshlet_index];
    const u32 triangle_index = local_thread_index.x;

    if(triangle_index >= meshlet.triangle_count) {
        return;
    } else {
        const f32vec2 scale = f32vec2(0.5, 0.5) * f32vec2(push.uses.u_globals->render_target_size) * f32(SUBPIXEL_SAMPLES);
        const f32vec2 bias = (0.5 * f32vec2(push.uses.u_globals->render_target_size)) * f32(SUBPIXEL_SAMPLES) + 0.5;
        f32mat4x4 pvm = mat_4x3_to_4x4(push.uses.u_transforms[meshlet_data.transform_index].model_matrix);
        if(push.uses.u_globals.render_as_observer) {
            pvm = mul(push.uses.u_globals->observer_camera.projection_matrix,
                    mul(push.uses.u_globals->observer_camera.view_matrix, pvm));
        } else {
            pvm = mul(push.uses.u_globals->main_camera.projection_matrix,
                    mul(push.uses.u_globals->main_camera.view_matrix, pvm));
        }
    
        #define stupid_indexing(matrix, i, j) matrix[j][i]
    
        stupid_indexing(pvm, 0, 0) = stupid_indexing(pvm, 0, 0) * scale.x + stupid_indexing(pvm, 0, 3) * bias.x;
        stupid_indexing(pvm, 1, 0) = stupid_indexing(pvm, 1, 0) * scale.x + stupid_indexing(pvm, 1, 3) * bias.x;
        stupid_indexing(pvm, 2, 0) = stupid_indexing(pvm, 2, 0) * scale.x + stupid_indexing(pvm, 2, 3) * bias.x;
        stupid_indexing(pvm, 3, 0) = stupid_indexing(pvm, 3, 0) * scale.x + stupid_indexing(pvm, 3, 3) * bias.x;
    
        stupid_indexing(pvm, 0, 1) = stupid_indexing(pvm, 0, 1) * scale.y + stupid_indexing(pvm, 0, 3) * bias.y;
        stupid_indexing(pvm, 1, 1) = stupid_indexing(pvm, 1, 1) * scale.y + stupid_indexing(pvm, 1, 3) * bias.y;
        stupid_indexing(pvm, 2, 1) = stupid_indexing(pvm, 2, 1) * scale.y + stupid_indexing(pvm, 2, 3) * bias.y;
        stupid_indexing(pvm, 3, 1) = stupid_indexing(pvm, 3, 1) * scale.y + stupid_indexing(pvm, 3, 3) * bias.y;
    
        #undef stupid_indexing
    
        const u32[3] indices = u32[3](
            get_vertex_index(mesh, meshlet, triangle_index, 0),
            get_vertex_index(mesh, meshlet, triangle_index, 1),
            get_vertex_index(mesh, meshlet, triangle_index, 2)
        );
    
        const f32vec3[3] positions = f32vec3[3](
            mesh.vertex_positions[indices[0]],
            mesh.vertex_positions[indices[1]],
            mesh.vertex_positions[indices[2]]
        );
    
        f32vec4[3] clip_position = f32vec4[3](
            mul(pvm, f32vec4(positions[0], 1.0f)),
            mul(pvm, f32vec4(positions[1], 1.0f)),
            mul(pvm, f32vec4(positions[2], 1.0f))
        );
    
        clip_position = f32vec4[3](
            clip_position[0] / clip_position[0].w,
            clip_position[1] / clip_position[1].w,
            clip_position[2] / clip_position[2].w,
        );

        f32vec2 ndc_min = min(min(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);
        f32vec2 ndc_max = max(max(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);

        if(is_triangle_invisible_micro_triangle( ndc_min, ndc_max, f32vec2(push.uses.u_globals->render_target_size))) { return; }
        if(is_triangle_backfacing(clip_position)) { return; }

        u32 data = 0;
        encode_triangle_id(data, meshlet_index, triangle_index);

        VisibilityImageSWFunction sw_function;
        sw_function.payload = data;
        sw_function.visibility_image = push.uses.u_visibility_image.get();
    
        rasterize(f32vec3[](f32vec3(floor(clip_position[0].xy), clip_position[0].z), 
                            f32vec3(floor(clip_position[1].xy), clip_position[1].z), 
                            f32vec3(floor(clip_position[2].xy), clip_position[2].z)), 
                            push.uses.u_globals->render_target_size, sw_function);
    }
}

[shader("fragment")]
void software_rasterization_frag(in Vertex vertex, in Primtive primitive) {}   
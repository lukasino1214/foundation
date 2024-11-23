#include "daxa/daxa.inl"
#include "software_rasterization.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/cull_util.slang"

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

groupshared u32 hw_meshlet_count;
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

#define SUBPIXEL_BITS 8
#define SUBPIXEL_SAMPLES (1 << SUBPIXEL_BITS)
#define greaterThan(X, Y) ((X) > (Y))
#define lessThan(X, Y) ((X) < (Y))

void rasterize(in f32vec3[3] triangle, in i32vec2 viewport_size, in u32 payload) {
    const f32vec3 v01 = triangle[1] - triangle[0];
    const f32vec3 v02 = triangle[2] - triangle[0];
    const f32 det_xy = v01.x * v02.y - v01.y * v02.x;
    if(det_xy >= 0.0) { return; }

    const f32 inv_det = 1.0 / det_xy;
    f32vec2 grad_z = f32vec2(
        (v01.z * v02.y - v01.y * v02.z) * inv_det,
        (v01.x * v02.z - v01.z * v02.x) * inv_det);

    f32vec2 vert_0 = triangle[0].xy;
    f32vec2 vert_1 = triangle[1].xy;
    f32vec2 vert_2 = triangle[2].xy;

    const f32vec2 min_subpixel = min(min(vert_0, vert_1), vert_2);
    const f32vec2 max_subpixel = max(max(vert_0, vert_1), vert_2);

    i32vec2 min_pixel = i32vec2(floor((min_subpixel + (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / f32(SUBPIXEL_SAMPLES))));
    i32vec2 max_pixel = i32vec2(floor((max_subpixel - (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / f32(SUBPIXEL_SAMPLES))));

    min_pixel = max(min_pixel, i32vec2(0));
    max_pixel = min(max_pixel, viewport_size.xy - 1);
    if(any(greaterThan(min_pixel, max_pixel))) {
        return;
    }

    max_pixel = min(max_pixel, min_pixel + 63);

    const f32vec2 edge_01 = -v01.xy;
    const f32vec2 edge_12 = vert_1 - vert_2;
    const f32vec2 edge_20 = v02.xy;

    const f32vec2 base_subpixel = f32vec2(min_pixel) * SUBPIXEL_SAMPLES + (SUBPIXEL_SAMPLES / 2);
    vert_0 -= base_subpixel;
    vert_1 -= base_subpixel;
    vert_2 -= base_subpixel;

    f32 hec_0 = edge_01.y * vert_0.x - edge_01.x * vert_0.y;
    f32 hec_1 = edge_12.y * vert_1.x - edge_12.x * vert_1.y;
    f32 hec_2 = edge_20.y * vert_2.x - edge_20.x * vert_2.y;

    hec_0 -= saturate(edge_01.y + saturate(1.0 - edge_01.x));
    hec_1 -= saturate(edge_12.y + saturate(1.0 - edge_12.x));
    hec_2 -= saturate(edge_20.y + saturate(1.0 - edge_20.x));

    const f32 z_0 = triangle[0].z - (grad_z.x * vert_0.x + grad_z.y * vert_0.y);
    grad_z *= SUBPIXEL_SAMPLES;

    f32 hec_y_0 = hec_0 * (1.0 / f32(SUBPIXEL_SAMPLES));
    f32 hec_y_1 = hec_1 * (1.0 / f32(SUBPIXEL_SAMPLES));
    f32 hec_y_2 = hec_2 * (1.0 / f32(SUBPIXEL_SAMPLES));
    f32 z_y = z_0;

    if(WaveActiveAnyTrue(max_pixel.x - min_pixel.x > 4)) {
        const f32vec3 edge_012 = f32vec3(edge_01.y, edge_12.y, edge_20.y);
        const bool3 is_open_edge = lessThan(edge_012, f32vec3(0.0));
        const f32vec3 inv_edge_012 = f32vec3(
            edge_012.x == 0 ? 1e8 : (1.0 / edge_012.x),
            edge_012.y == 0 ? 1e8 : (1.0 / edge_012.y),
            edge_012.z == 0 ? 1e8 : (1.0 / edge_012.z));
        i32 y = min_pixel.y;
        while (true) {
            const f32vec3 cross_x = f32vec3(hec_y_0, hec_y_1, hec_y_2) * inv_edge_012;
            const f32vec3 min_x = f32vec3(
                is_open_edge.x ? cross_x.x : 0.0,
                is_open_edge.y ? cross_x.y : 0.0,
                is_open_edge.z ? cross_x.z : 0.0);
            const f32vec3 max_x = f32vec3(
                is_open_edge.x ? max_pixel.x - min_pixel.x : cross_x.x,
                is_open_edge.y ? max_pixel.x - min_pixel.x : cross_x.y,
                is_open_edge.z ? max_pixel.x - min_pixel.x : cross_x.z);
            f32 x_0 = ceil(max(max(min_x.x, min_x.y), min_x.z));
            f32 x_1 = min(min(max_x.x, max_x.y), max_x.z);
            f32 z_x = z_y + grad_z.x * x_0;

            x_0 += min_pixel.x;
            x_1 += min_pixel.x;
            for (f32 x = x_0; x <= x_1; ++x) {
                AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_x));
                z_x += grad_z.x;
            }

            if (y >= max_pixel.y) { break; }
            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    } else {
        i32 y = min_pixel.y;
        while (true) {
            i32 x = min_pixel.x;
            if (min(min(hec_y_0, hec_y_1), hec_y_2) >= 0.0) {
                AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_y));
            }

            if (x < max_pixel.x) {
                f32 hec_x_0 = hec_y_0 - edge_01.y;
                f32 hec_x_1 = hec_y_1 - edge_12.y;
                f32 hec_x_2 = hec_y_2 - edge_20.y;
                f32 z_x = z_y + grad_z.x;
                ++x;

                while (true) {
                    if (min(min(hec_x_0, hec_x_1), hec_x_2) >= 0.0) {
                        AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_x));
                    }

                    if (x >= max_pixel.x) { break; }

                    hec_x_0 -= edge_01.y;
                    hec_x_1 -= edge_12.y;
                    hec_x_2 -= edge_20.y;
                    z_x += grad_z.x;
                    ++x;
                }
            }

            if (y >= max_pixel.y) { break; }

            hec_y_0 += edge_01.x;
            hec_y_1 += edge_12.x;
            hec_y_2 += edge_20.x;
            z_y += grad_z.y;
            ++y;
        }
    }
}

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
        const f32mat4x4 transform_matrix = push.uses.u_transforms[meshlet_data.transform_index].model_matrix; 
        f32mat4x4 pvm = push.uses.u_transforms[meshlet_data.transform_index].model_matrix;
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
        let cull_micro_poly_invisible = is_triangle_invisible_micro_triangle( ndc_min, ndc_max, f32vec2(push.uses.u_globals->render_target_size));

        if(cull_micro_poly_invisible) { return; }

        let cull_backfacing_triangle = is_triangle_backfacing(clip_position);

        if(cull_backfacing_triangle) { return; }

        u32 data = 0;
        encode_triangle_id(data, meshlet_index, triangle_index);
    
        rasterize(f32vec3[](f32vec3(floor(clip_position[0].xy), clip_position[0].z), 
                            f32vec3(floor(clip_position[1].xy), clip_position[1].z), 
                            f32vec3(floor(clip_position[2].xy), clip_position[2].z)), 
                            push.uses.u_globals->render_target_size, data);
    }
}

[shader("fragment")]
void software_rasterization_frag(in Vertex vertex, in Primtive primitive) {}   
#pragma once
#include "glue.inl"
#include "shared.inl"
#include "visibility_buffer.slang"
#include "meshlet.slang"
#include "cull_util.slang"

#define SUBPIXEL_BITS 8
#define SUBPIXEL_SAMPLES (1 << SUBPIXEL_BITS)

// interface SWRasterFunction {
//     void write(u32vec2 position, f32 depth);
// };

// struct VisibilityImageSWFunction : SWRasterFunction {
//     u32 payload;
//     RWTexture2D<u64> visibility_image;
//     RWTexture2D<u32> overdraw_image;

//     void write(u32vec2 position, f32 depth) {
//         AtomicMaxU64(visibility_image[position], encode_visibility_buffer_payload(payload, depth));
// #if OVERDRAW_DEBUG
//         InterlockedAdd(overdraw_image[position], 1);
// #endif
//     }
// };

// struct OnlyDepthSWFunction : SWRasterFunction {
//     RWTexture2D<u32> depth_image;

//     void write(u32vec2 position, f32 depth) {
//         AtomicMaxU32(depth_image[position], asuint(depth));
//     }
// };

// void rasterize<T : SWRasterFunction>(in f32vec3[3] vertices, in i32vec2 viewport_size, T sw_function) {
//     const f32vec3 v01 = vertices[1] - vertices[0];
//     const f32vec3 v02 = vertices[2] - vertices[0];
//     const f32 det_xy = v01.x * v02.y - v01.y * v02.x;
//     if(det_xy >= 0.0) { return; }

//     const f32 inv_det = 1.0 / det_xy;
//     f32vec2 grad_z = f32vec2(
//         (v01.z * v02.y - v01.y * v02.z) * inv_det,
//         (v01.x * v02.z - v01.z * v02.x) * inv_det);

//     f32vec2 vert_0 = vertices[0].xy;
//     f32vec2 vert_1 = vertices[1].xy;
//     f32vec2 vert_2 = vertices[2].xy;

//     const f32vec2 min_subpixel = min(min(vert_0, vert_1), vert_2);
//     const f32vec2 max_subpixel = max(max(vert_0, vert_1), vert_2);

//     i32vec2 min_pixel = i32vec2(floor((min_subpixel + (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / f32(SUBPIXEL_SAMPLES))));
//     i32vec2 max_pixel = i32vec2(floor((max_subpixel - (SUBPIXEL_SAMPLES / 2) - 1) * (1.0 / f32(SUBPIXEL_SAMPLES))));

//     min_pixel = max(min_pixel, i32vec2(0));
//     max_pixel = min(max_pixel, viewport_size.xy - 1);
//     if(any(greaterThan(min_pixel, max_pixel))) {
//         return;
//     }

//     max_pixel = min(max_pixel, min_pixel + 63);

//     const f32vec2 edge_01 = -v01.xy;
//     const f32vec2 edge_12 = vert_1 - vert_2;
//     const f32vec2 edge_20 = v02.xy;

//     const f32vec2 base_subpixel = f32vec2(min_pixel) * SUBPIXEL_SAMPLES + (SUBPIXEL_SAMPLES / 2);
//     vert_0 -= base_subpixel;
//     vert_1 -= base_subpixel;
//     vert_2 -= base_subpixel;

//     f32 hec_0 = edge_01.y * vert_0.x - edge_01.x * vert_0.y;
//     f32 hec_1 = edge_12.y * vert_1.x - edge_12.x * vert_1.y;
//     f32 hec_2 = edge_20.y * vert_2.x - edge_20.x * vert_2.y;

//     hec_0 -= saturate(edge_01.y + saturate(1.0 - edge_01.x));
//     hec_1 -= saturate(edge_12.y + saturate(1.0 - edge_12.x));
//     hec_2 -= saturate(edge_20.y + saturate(1.0 - edge_20.x));

//     const f32 z_0 = vertices[0].z - (grad_z.x * vert_0.x + grad_z.y * vert_0.y);
//     grad_z *= SUBPIXEL_SAMPLES;

//     f32 hec_y_0 = hec_0 * (1.0 / f32(SUBPIXEL_SAMPLES));
//     f32 hec_y_1 = hec_1 * (1.0 / f32(SUBPIXEL_SAMPLES));
//     f32 hec_y_2 = hec_2 * (1.0 / f32(SUBPIXEL_SAMPLES));
//     f32 z_y = z_0;

//     if(WaveActiveAnyTrue(max_pixel.x - min_pixel.x > 4)) {
//         const f32vec3 edge_012 = f32vec3(edge_01.y, edge_12.y, edge_20.y);
//         const bool3 is_open_edge = lessThan(edge_012, f32vec3(0.0));
//         const f32vec3 inv_edge_012 = f32vec3(
//             edge_012.x == 0 ? 1e8 : (1.0 / edge_012.x),
//             edge_012.y == 0 ? 1e8 : (1.0 / edge_012.y),
//             edge_012.z == 0 ? 1e8 : (1.0 / edge_012.z));
//         i32 y = min_pixel.y;
//         while (true) {
//             const f32vec3 cross_x = f32vec3(hec_y_0, hec_y_1, hec_y_2) * inv_edge_012;
//             const f32vec3 min_x = f32vec3(
//                 is_open_edge.x ? cross_x.x : 0.0,
//                 is_open_edge.y ? cross_x.y : 0.0,
//                 is_open_edge.z ? cross_x.z : 0.0);
//             const f32vec3 max_x = f32vec3(
//                 is_open_edge.x ? max_pixel.x - min_pixel.x : cross_x.x,
//                 is_open_edge.y ? max_pixel.x - min_pixel.x : cross_x.y,
//                 is_open_edge.z ? max_pixel.x - min_pixel.x : cross_x.z);
//             f32 x_0 = ceil(max(max(min_x.x, min_x.y), min_x.z));
//             f32 x_1 = min(min(max_x.x, max_x.y), max_x.z);
//             f32 z_x = z_y + grad_z.x * x_0;

//             x_0 += min_pixel.x;
//             x_1 += min_pixel.x;
//             for (f32 x = x_0; x <= x_1; ++x) {
//                 sw_function.write(u32vec2(x, y), z_x);
//                 printf("%f %f", x, y);
//                 z_x += grad_z.x;
//             }

//             if (y >= max_pixel.y) { break; }
//             hec_y_0 += edge_01.x;
//             hec_y_1 += edge_12.x;
//             hec_y_2 += edge_20.x;
//             z_y += grad_z.y;
//             ++y;
//         }
//     } else {
//         i32 y = min_pixel.y;
//         while (true) {
//             i32 x = min_pixel.x;
//             if (min(min(hec_y_0, hec_y_1), hec_y_2) >= 0.0) {
//                 sw_function.write(u32vec2(x, y), z_y);
//             }

//             if (x < max_pixel.x) {
//                 f32 hec_x_0 = hec_y_0 - edge_01.y;
//                 f32 hec_x_1 = hec_y_1 - edge_12.y;
//                 f32 hec_x_2 = hec_y_2 - edge_20.y;
//                 f32 z_x = z_y + grad_z.x;
//                 ++x;

//                 while (true) {
//                     if (min(min(hec_x_0, hec_x_1), hec_x_2) >= 0.0) {
//                         sw_function.write(u32vec2(x, y), z_x);
//                         printf("%f %f", x, y);
//                     }

//                     if (x >= max_pixel.x) { break; }

//                     hec_x_0 -= edge_01.y;
//                     hec_x_1 -= edge_12.y;
//                     hec_x_2 -= edge_20.y;
//                     z_x += grad_z.x;
//                     ++x;
//                 }
//             }

//             if (y >= max_pixel.y) { break; }

//             hec_y_0 += edge_01.x;
//             hec_y_1 += edge_12.x;
//             hec_y_2 += edge_20.x;
//             z_y += grad_z.y;
//             ++y;
//         }
//     }
// }

// #define OPTIMIZATION 1

// #if OPTIMIZATION == 1
// groupshared f32vec4 shared_positions[MAX_VERTICES_PER_MESHLET];
// #endif

// void generic_rasterization<T: SWRasterFunction>(
//     in u32 thread_index, 
//     in u32 meshlet_index, 
//     in Mesh mesh, 
//     in Meshlet meshlet, 
//     in f32mat4x4 pvm, 
//     u32vec2 image_size, 
//     T sw_function) {
//     const f32vec2 scale = f32vec2(0.5, 0.5) * f32vec2(image_size) * f32(SUBPIXEL_SAMPLES);
//     const f32vec2 bias = (0.5 * f32vec2(image_size)) * f32(SUBPIXEL_SAMPLES) + 0.5;
    
//     #define stupid_indexing(matrix, i, j) matrix[j][i]
    
//     stupid_indexing(pvm, 0, 0) = stupid_indexing(pvm, 0, 0) * scale.x + stupid_indexing(pvm, 0, 3) * bias.x;
//     stupid_indexing(pvm, 1, 0) = stupid_indexing(pvm, 1, 0) * scale.x + stupid_indexing(pvm, 1, 3) * bias.x;
//     stupid_indexing(pvm, 2, 0) = stupid_indexing(pvm, 2, 0) * scale.x + stupid_indexing(pvm, 2, 3) * bias.x;
//     stupid_indexing(pvm, 3, 0) = stupid_indexing(pvm, 3, 0) * scale.x + stupid_indexing(pvm, 3, 3) * bias.x;

//     stupid_indexing(pvm, 0, 1) = stupid_indexing(pvm, 0, 1) * scale.y + stupid_indexing(pvm, 0, 3) * bias.y;
//     stupid_indexing(pvm, 1, 1) = stupid_indexing(pvm, 1, 1) * scale.y + stupid_indexing(pvm, 1, 3) * bias.y;
//     stupid_indexing(pvm, 2, 1) = stupid_indexing(pvm, 2, 1) * scale.y + stupid_indexing(pvm, 2, 3) * bias.y;
//     stupid_indexing(pvm, 3, 1) = stupid_indexing(pvm, 3, 1) * scale.y + stupid_indexing(pvm, 3, 3) * bias.y;

//     #undef stupid_indexing

// #if OPTIMIZATION
//     if(thread_index < meshlet.vertex_count) {
//         shared_positions[thread_index] = mul(pvm, f32vec4(mesh.vertex_positions[mesh.indirect_vertices[meshlet.indirect_vertex_offset + thread_index]], 1.0f));
//     }

//     GroupMemoryBarrierWithGroupSync();

//     if(thread_index < meshlet.triangle_count) {
//         f32vec4[3] clip_position = f32vec4[3](
//             shared_positions[get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0)],
//             shared_positions[get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1)],
//             shared_positions[get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)]
//         );
    
//         clip_position = f32vec4[3](
//             clip_position[0] * rcp(clip_position[0].w),
//             clip_position[1] * rcp(clip_position[1].w),
//             clip_position[2] * rcp(clip_position[2].w),
//         );

//         const f32vec2 ndc_min = min(min(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);
//         const f32vec2 ndc_max = max(max(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);

//         if(is_triangle_invisible_micro_triangle( ndc_min, ndc_max, f32vec2(image_size))) { return; }
//         if(is_triangle_backfacing(clip_position)) { return; }

//         if(T is VisibilityImageSWFunction) {
//             var vis_sw_function = reinterpret<VisibilityImageSWFunction>(sw_function);
//             encode_triangle_id(vis_sw_function.payload, meshlet_index, thread_index);
//             sw_function = reinterpret<T>(vis_sw_function);
//         } 

//         rasterize<T>(f32vec3[](f32vec3(floor(clip_position[0].xy), clip_position[0].z), 
//                                f32vec3(floor(clip_position[1].xy), clip_position[1].z), 
//                                f32vec3(floor(clip_position[2].xy), clip_position[2].z)), 
//                                image_size, sw_function);
//     }
// #else
//     if(thread_index < meshlet.triangle_count) {
//         const u32[3] indices = u32[3](
//             get_vertex_index(mesh, meshlet, thread_index, 0),
//             get_vertex_index(mesh, meshlet, thread_index, 1),
//             get_vertex_index(mesh, meshlet, thread_index, 2)
//         );
    
//         const f32vec3[3] positions = f32vec3[3](
//             mesh.vertex_positions[indices[0]],
//             mesh.vertex_positions[indices[1]],
//             mesh.vertex_positions[indices[2]]
//         );
    
//         f32vec4[3] clip_position = f32vec4[3](
//             mul(pvm, f32vec4(positions[0], 1.0f)),
//             mul(pvm, f32vec4(positions[1], 1.0f)),
//             mul(pvm, f32vec4(positions[2], 1.0f))
//         );
    
//         clip_position = f32vec4[3](
//             clip_position[0] * rcp(clip_position[0].w),
//             clip_position[1] * rcp(clip_position[1].w),
//             clip_position[2] * rcp(clip_position[2].w),
//         );

//         f32vec2 ndc_min = min(min(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);
//         f32vec2 ndc_max = max(max(clip_position[0].xy, clip_position[1].xy), clip_position[2].xy);

//         if(is_triangle_invisible_micro_triangle( ndc_min, ndc_max, f32vec2(image_size))) { return; }
//         if(is_triangle_backfacing(clip_position)) { return; }

//         if(T is VisibilityImageSWFunction) {
//             var vis_sw_function = reinterpret<VisibilityImageSWFunction>(sw_function);
//             encode_triangle_id(vis_sw_function.payload, meshlet_index, thread_index);
//             sw_function = reinterpret<T>(vis_sw_function);
//         } 

//         rasterize<T>(f32vec3[](f32vec3(floor(clip_position[0].xy), clip_position[0].z), 
//                                f32vec3(floor(clip_position[1].xy), clip_position[1].z), 
//                                f32vec3(floor(clip_position[2].xy), clip_position[2].z)), 
//                                image_size, sw_function);
//     }
// #endif
// }
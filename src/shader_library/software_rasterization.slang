#pragma once
#include "glue.inl"
#include "shared.inl"
#include "visibility_buffer.slang"

#define SUBPIXEL_BITS 8
#define SUBPIXEL_SAMPLES (1 << SUBPIXEL_BITS)

interface SWRasterFunction {
    void write(u32vec2 position, f32 depth);
};

void rasterize<T : SWRasterFunction>(in f32vec3[3] vertices, in i32vec2 viewport_size, T sw_function) {
    const f32vec3 v01 = vertices[1] - vertices[0];
    const f32vec3 v02 = vertices[2] - vertices[0];
    const f32 det_xy = v01.x * v02.y - v01.y * v02.x;
    if(det_xy >= 0.0) { return; }

    const f32 inv_det = 1.0 / det_xy;
    f32vec2 grad_z = f32vec2(
        (v01.z * v02.y - v01.y * v02.z) * inv_det,
        (v01.x * v02.z - v01.z * v02.x) * inv_det);

    f32vec2 vert_0 = vertices[0].xy;
    f32vec2 vert_1 = vertices[1].xy;
    f32vec2 vert_2 = vertices[2].xy;

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

    const f32 z_0 = vertices[0].z - (grad_z.x * vert_0.x + grad_z.y * vert_0.y);
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
                // AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_x));
                sw_function.write(u32vec2(x, y), z_x);
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
                // AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_y));
                sw_function.write(u32vec2(x, y), z_y);
            }

            if (x < max_pixel.x) {
                f32 hec_x_0 = hec_y_0 - edge_01.y;
                f32 hec_x_1 = hec_y_1 - edge_12.y;
                f32 hec_x_2 = hec_y_2 - edge_20.y;
                f32 z_x = z_y + grad_z.x;
                ++x;

                while (true) {
                    if (min(min(hec_x_0, hec_x_1), hec_x_2) >= 0.0) {
                        // AtomicMaxU64(push.uses.u_visibility_image.get()[u32vec2(x, y)], encode_visibility_buffer_payload(payload, z_x));
                        sw_function.write(u32vec2(x, y), z_x);
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
#pragma once
#include "glue.inl"

auto octahedron_wrap(const_ref(f32vec2) v) -> f32vec2 {
    f32vec2 w = 1.0f - abs(f32vec2(v.y, v.x));
	if (v.x < 0.0f) w.x = -w.x;
	if (v.y < 0.0f) w.y = -w.y;
	return w;
}

auto encode_normal(const_ref(f32vec3) v) -> u32 {
    f32vec3 n = v;
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n = f32vec3(n.z > 0.0f ? f32vec2(n.x, n.y) : octahedron_wrap(f32vec2(n.x, n.y)), n.z);
	n = f32vec3(f32vec2(n.x, n.y) * 0.5f + 0.5f, n.z);

	return pack_snorm_2x16(f32vec2(n.x, n.y));
}

auto decode_normal(u32 v) -> f32vec3 {
    f32vec2 f = unpack_snorm_2x16(v);
    f = f * 2.0f - 1.0f;

	f32vec3 n = f32vec3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));
	f32 t = max(-n.z, 0.0f);
	n.x += n.x >= 0.0f ? -t : t;
	n.y += n.y >= 0.0f ? -t : t;

	return normalize(n);
}

auto encode_uv(const_ref(f32vec2) v) -> u32 {
    return pack_half_2x16(v);
} 

auto decode_uv(u32 v) -> f32vec2 {
    return unpack_half_2x16(v);
}
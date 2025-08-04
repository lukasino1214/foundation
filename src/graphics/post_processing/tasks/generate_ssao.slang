#include "daxa/daxa.inl"
#include "generate_ssao.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/shading.slang"

#define KERNEL_SIZE 26

const static f32vec3 kernel_samples[KERNEL_SIZE] = {
    f32vec3(0.2196607,0.9032637,0.2254677),
    f32vec3(0.05916681,0.2201506,0.1430302),
    f32vec3(-0.4152246,0.1320857,0.7036734),
    f32vec3(-0.3790807,0.1454145,0.100605),
    f32vec3(0.3149606,-0.1294581,0.7044517),
    f32vec3(-0.1108412,0.2162839,0.1336278),
    f32vec3(0.658012,-0.4395972,0.2919373),
    f32vec3(0.5377914,0.3112189,0.426864),
    f32vec3(-0.2752537,0.07625949,0.1273409),
    f32vec3(-0.1915639,-0.4973421,0.3129629),
    f32vec3(-0.2634767,0.5277923,0.1107446),
    f32vec3(0.8242752,0.02434147,0.06049098),
    f32vec3(0.06262707,-0.2128643,0.03671562),
    f32vec3(-0.1795662,-0.3543862,0.07924347),
    f32vec3(0.06039629,0.24629,0.4501176),
    f32vec3(-0.7786345,-0.3814852,0.2391262),
    f32vec3(0.2792919,0.2487278,0.05185341),
    f32vec3(0.1841383,0.1696993,0.8936281),
    f32vec3(-0.3479781,0.4725766,0.719685),
    f32vec3(-0.1365018,-0.2513416,0.470937),
    f32vec3(0.1280388,-0.563242,0.3419276),
    f32vec3(-0.4800232,-0.1899473,0.2398808),
    f32vec3(0.6389147,0.1191014,0.5271206),
    f32vec3(0.1932822,-0.3692099,0.6060588),
    f32vec3(-0.3465451,-0.1654651,0.6746758),
    f32vec3(0.2448421,-0.1610962,0.1289366)
};

f32 rand(f32vec2 c) {
	return fract(sin(dot(c.xy, f32vec2(12.9898, 78.233))) * 43758.5453);
}

f32 noise(f32vec2 p, f32 freq) {
	f32 unit = 2560 / freq;
	f32vec2 ij = floor(p / unit);
	f32vec2 xy = fmod(p, unit) / unit;
	xy = .5 * (1. - cos(3.14159265359 * xy));
	f32 a = rand((ij + f32vec2(0., 0.)));
	f32 b = rand((ij + f32vec2(1., 0.)));
	f32 c = rand((ij + f32vec2(0., 1.)));
	f32 d = rand((ij + f32vec2(1., 1.)));
	f32 x1 = lerp(a, b, xy.x);
	f32 x2 = lerp(c, d, xy.x);
	return lerp(x1, x2, xy.y);
}

f32vec3 get_view_position_from_depth(f32vec2 uv, f32 depth) {
    f32vec4 clip_space_position = f32vec4(uv * 2.0 - 1.0, depth, 1.0);
    f32vec4 view_space_position = mul(push.uses.u_globals.main_camera.inverse_projection_matrix, clip_space_position);
    return view_space_position.xyz / view_space_position.w;
}

#define RADIUS 0.3f
#define BIAS  0.025f

[[vk::push_constant]] GenerateSSAOPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void generate_ssao(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_half_size)) { return; }

    const f32 depth = push.uses.u_depth_half_res_image.get()[pixel.xy];

    if(depth == 0.0) {
        push.uses.u_ssao_image.get()[pixel.xy] = 0.0f;
        return;
    }

    const f32vec2 uv = f32vec2(pixel.xy + 0.5f) * push.uses.u_globals.render_target_half_size_inv;
    const i32vec2 noise_dim = push.uses.u_globals.render_target_half_size;

    const f32vec3 random_vec = normalize(f32vec3(
        noise(uv, noise_dim.x * 2),
        noise(pow(uv, f32vec2(1.1)), pow(noise_dim.x * 4.2, 1.5 + uv.x / 10.0)),
        0.0
    ));

    const f32vec3 world_space_normal = decode_normal(push.uses.u_normal_detail_half_res_image.get()[pixel.xy]);
    const f32vec3 normal = mul(f32mat3x3(push.uses.u_globals.main_camera.view_matrix), world_space_normal.xyz);

    const f32vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
	const f32vec3 bitangent = cross(tangent, normal);
	const f32mat3x3 tbn = transpose(f32mat3x3(tangent, bitangent, normal));

    const f32vec3 frag_position = pixel_index_to_view_space(push.uses.u_globals.main_camera, push.uses.u_globals.render_target_half_size_inv, f32vec2(pixel.xy), depth);

	f32 occlusion = 0.0f;
	for(i32 i = 0; i < KERNEL_SIZE; i++) {		
		f32vec3 sample_pos = mul(tbn, kernel_samples[i].xyz); 
		sample_pos = frag_position + sample_pos * RADIUS; 
		
		f32vec4 offset = f32vec4(sample_pos, 1.0f);
		offset = mul(push.uses.u_globals.main_camera.projection_matrix, offset); 
		offset.xy /= offset.w; 
		offset.xy = offset.xy * 0.5f + 0.5f; 
		
        f32 found_depth = push.uses.u_depth_half_res_image.get()[offset.xy * push.uses.u_globals.render_target_half_size];
		f32vec3 sample_depth_v = get_view_position_from_depth(offset.xy, found_depth);
        f32 sample_depth = sample_depth_v.z;

		f32 range_check = smoothstep(0.0f, 1.0f, RADIUS / abs(frag_position.z - sample_depth));
		occlusion += (sample_depth >= sample_pos.z + BIAS ? 1.0f : 0.0f) * range_check;
    }

	occlusion = 1.0 - (occlusion / f32(KERNEL_SIZE));

    push.uses.u_ssao_image.get()[pixel.xy] = occlusion;
}
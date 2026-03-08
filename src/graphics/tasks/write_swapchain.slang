#include <daxa/daxa.inl>
#include "write_swapchain.inl"

#define SRGB_2_XYZ_MAT \
transpose(f32mat3x3( \
	0.4124564, 0.3575761, 0.1804375, \
    0.2126729, 0.7151522, 0.0721750, \
    0.0193339, 0.1191920, 0.9503041 \
))

#define AXG_TRANSFORM \
 transpose(f32mat3x3(\
    0.842479062253094 , 0.0423282422610123, 0.0423756549057051, \
    0.0784335999999992, 0.878468636469772 , 0.0784336, \
    0.0792237451477643, 0.0791661274605434, 0.879142973793104 \
))

#define INV_AGX_TRANSFORM \
transpose(f32mat3x3( \
     1.19687900512017  , -0.0528968517574562, -0.0529716355144438, \
    -0.0980208811401368,  1.15190312990417  , -0.0980434501171241, \
    -0.0990297440797205, -0.0989611768448433,  1.15107367264116 \
))

#define XYZ_2_SRGB_MAT \
transpose(f32mat3x3( \
	 3.2409699419,-1.5373831776,-0.4986107603, \
    -0.9692436363, 1.8759675015, 0.0415550574, \
	 0.0556300797,-0.2039769589, 1.0569715142 \
))

f32 luminance_from_color(f32vec3 color) {
    f32vec3 luminance_coefficients = transpose(SRGB_2_XYZ_MAT)[1];
    return dot(color, luminance_coefficients);
}

f32vec3 agxDefaultContrastApproximation(f32vec3 x) {
	f32vec3 x2 = x * x;
	f32vec3 x4 = x2 * x2;
	return +15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
void agxLook(inout f32vec3 color) {
	const f32vec3 slope      = (1.1f).xxx;
	const f32vec3 power      = (1.2f).xxx;
	const f32 saturation  = 1.3;
	f32 luma = luminance_from_color(color);
	color = pow(color * slope, power);
	color = max(luma + saturation * (color - luma), f32vec3(0.0));
}

f32vec3 agx_tonemapping(f32vec3 color) {
	color *= 0.28;
	// AgX constants
	const f32mat3x3 AgXInsetMatrix = transpose(f32mat3x3(
		f32vec3(0.856627153315983, 0.137318972929847, 0.11189821299995),
		f32vec3(0.0951212405381588, 0.761241990602591, 0.0767994186031903),
		f32vec3(0.0482516061458583, 0.101439036467562, 0.811302368396859)));
	// explicit AgXOutsetMatrix generated from Filaments AgXOutsetMatrixInv
	const f32mat3x3 AgXOutsetMatrix = transpose(f32mat3x3(
		f32vec3(1.1271005818144368, -0.1413297634984383, -0.14132976349843826),
		f32vec3(-0.11060664309660323, 1.157823702216272, -0.11060664309660294),
		f32vec3(-0.016493938717834573, -0.016493938717834257, 1.2519364065950405)));
	const f32mat3x3 LINEAR_REC2020_TO_LINEAR_SRGB = transpose(f32mat3x3(
		f32vec3(1.6605, -0.1246, -0.0182),
		f32vec3(-0.5876, 1.1329, -0.1006),
		f32vec3(-0.0728, -0.0083, 1.1187)));
	const f32mat3x3 LINEAR_SRGB_TO_LINEAR_REC2020 = transpose(f32mat3x3(
		f32vec3(0.6274, 0.0691, 0.0164),
		f32vec3(0.3293, 0.9195, 0.0880),
		f32vec3(0.0433, 0.0113, 0.8956)));
	// LOG2_MIN      = -10.0
	// LOG2_MAX      =  +6.5
	// MIDDLE_GRAY   =  0.18
	const f32 AgxMinEv = -12.47393; // log2( pow( 2, LOG2_MIN ) * MIDDLE_GRAY )
	const f32 AgxMaxEv = 4.026069;  // log2( pow( 2, LOG2_MAX ) * MIDDLE_GRAY )
	color = mul(LINEAR_SRGB_TO_LINEAR_REC2020, color);
	color = mul(AgXInsetMatrix, color);
	// Log2 encoding
	color = max(color, 1e-10); // avoid 0 or negative numbers for log2
	color = log2(color);
	color = (color - AgxMinEv) / (AgxMaxEv - AgxMinEv);
	color = clamp(color, 0.0, 1.0);
	// Apply sigmoid
	color = agxDefaultContrastApproximation(color);
	// Apply AgX look
	agxLook(color);
	color = mul(AgXOutsetMatrix, color);
	color = mul(LINEAR_REC2020_TO_LINEAR_SRGB, color);
	// Gamut mapping. Simple clamp for now.
	color = clamp(color, 0.0, 1.0);
	return color;
}

f32 sRGB_OETF(f32 a) {
    return select(.0031308f >= a, 12.92f * a, 1.055f * pow(a, .4166666666666667f) - .055f);
}

f32vec3 sRGB_OETF(f32vec3 a) {
    return f32vec3(sRGB_OETF(a.r), sRGB_OETF(a.g), sRGB_OETF(a.b));
}

[[vk::push_constant]] WriteSwapchainPush push;
[numthreads(16, 8, 1)]
void write_swapchain(u32vec2 index : SV_DispatchThreadID) {
    if(any(greaterThanEqual(index, push.size))) { return; }

    const f32vec4 exposed_color = push.uses.u_render_image.get()[index];
    const f32vec3 tonemapped_color = agx_tonemapping(exposed_color.rgb);
    const f32vec3 gamma_correct = sRGB_OETF(tonemapped_color);

    push.uses.u_swapchain_image.get()[index] = f32vec4(gamma_correct.rgb, 1.0f);
}
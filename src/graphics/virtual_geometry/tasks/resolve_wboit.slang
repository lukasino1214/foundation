#include "daxa/daxa.inl"
#include "resolve_wboit.inl"
#include "../../../shader_library/visibility_buffer.slang"

[[vk::push_constant]] ResolveWBOITPush push;

struct PixelInfo {
    f32vec2 uv;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
PixelInfo resolve_wboit_vert(const u32 vertex_index : SV_VertexID) {
    PixelInfo info;
    info.uv = f32vec2((vertex_index << 1) & 2, vertex_index & 2);
    info.sv_position = f32vec4(info.uv * 2.0 - 1.0, 0.0, 1.0);
    return info;
}

[shader("fragment")]
f32vec4 resolve_wboit_frag(f32vec2 uv: f32vec2, f32vec4 pixel_position: SV_Position) {
    f32vec4 accumulation = push.uses.u_wboit_accumulation_image.get()[pixel_position.xy];
    f32 reveal = push.uses.u_wboit_reveal_image.get()[pixel_position.xy];

    return f32vec4(accumulation.rgb / clamp(accumulation.a, 1e-4, 5e4), 1.0 - reveal);
}
#include "daxa/daxa.inl"
#include "extract_depth.inl"
#include "../../../shader_library/visibility_buffer.slang"

[[vk::push_constant]] ExtractDepthPush push;

struct PixelInfo {
    f32vec2 uv;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
PixelInfo extract_depth_vert(const u32 vertex_index : SV_VertexID) {
    PixelInfo info;
    info.uv = f32vec2((vertex_index << 1) & 2, vertex_index & 2);
    info.sv_position = f32vec4(info.uv * 2.0 - 1.0, 0.0, 1.0);
    return info;
}

struct DepthInfo {
    f32 depth : SV_Depth;
};

[shader("fragment")]
DepthInfo extract_depth_frag(f32vec2 uv: f32vec2, f32vec4 pixel_position: SV_Position) {
    u32 vis_index = 0;
    u32 depth = 0;
    decode_visibility_buffer_payload(tex_rw_u64_table[push.uses.u_visibility_image.index()][pixel_position.xy], vis_index, depth);

    DepthInfo info;
    info.depth = asfloat(depth);
    return info;
}   
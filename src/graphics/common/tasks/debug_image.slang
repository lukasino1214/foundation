#include "daxa/daxa.inl"
#include "debug_image.inl"
#include "../../../shader_library/vertex_compression.inl"

[[vk::push_constant]] DebugImagePush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void debug_image(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }
    f32vec3 value = f32vec3(0);
    value = f32vec3(push.uses.u_f32_image.get()[pixel.xy]);
    // value = decode_normal(push.uses.u_u32_image.get()[pixel.xy]);

    push.uses.u_color_image.get()[pixel.xy] = f32vec4(value, 1.0f);
}
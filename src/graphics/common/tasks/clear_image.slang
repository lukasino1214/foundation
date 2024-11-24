#include "daxa/daxa.inl"
#include "clear_image.inl"

[[vk::push_constant]] ClearImagePush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void clear_image(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }
    RWTexture2D<f32vec4> image = RWTexture2D<f32vec4>::get(push.uses.u_image);
    image[pixel.xy] = f32vec4(push.color, 1.0f);
}
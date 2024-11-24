#include "daxa/daxa.inl"
#include "combine_depth.inl"

[[vk::push_constant]] CombineDepthPush push;
[shader("compute")]
[numthreads(16, 16, 1)]
void combine_depth(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= push.uses.u_globals->render_target_size)) { return; }

    push.uses.u_depth_image_f32.get()[pixel.xy] = max(push.uses.u_depth_image_d32.get()[pixel.xy], asfloat(push.uses.u_depth_image_u32.get()[pixel.xy]));
}
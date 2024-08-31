#include "daxa/daxa.inl"
#include "debug.inl"

#if defined(DebugDraw_SHADER)
[[vk::push_constant]] DebugDrawPush push;

struct VertexStageOutput {
    f32vec3 color : f32vec3;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const f32vec3 aabb_vertex_base_offsets[24] = f32vec3[24](
        // Bottom rectangle:
        f32vec3(-0.5, -0.5, -0.5), f32vec3(+0.5, -0.5, -0.5),
        f32vec3(-0.5, -0.5, -0.5), f32vec3(-0.5, +0.5, -0.5),
        f32vec3(-0.5, +0.5, -0.5), f32vec3(+0.5, +0.5, -0.5),
        f32vec3(+0.5, -0.5, -0.5), f32vec3(+0.5, +0.5, -0.5),
        // Top rectangle:
        f32vec3(-0.5, -0.5, +0.5), f32vec3(+0.5, -0.5, +0.5),
        f32vec3(-0.5, -0.5, +0.5), f32vec3(-0.5, +0.5, +0.5),
        f32vec3(-0.5, +0.5, +0.5), f32vec3(+0.5, +0.5, +0.5),
        f32vec3(+0.5, -0.5, +0.5), f32vec3(+0.5, +0.5, +0.5),
        // Connecting lines:
        f32vec3(-0.5, -0.5, -0.5), f32vec3(-0.5, -0.5, +0.5),
        f32vec3(+0.5, -0.5, -0.5), f32vec3(+0.5, -0.5, +0.5),
        f32vec3(-0.5, +0.5, -0.5), f32vec3(-0.5, +0.5, +0.5),
        f32vec3(+0.5, +0.5, -0.5), f32vec3(+0.5, +0.5, +0.5)
    );

    const ShaderDebugAABBDraw aabb = push.uses.u_globals->debug->aabb_draws[instance_index];

    VertexStageOutput output;
    output.color = aabb.color;
    output.sv_position = mul(push.uses.u_globals->camera_projection_matrix,
                         mul(push.uses.u_globals->camera_view_matrix,
                         mul(push.uses.u_transforms[aabb.transform_index].model_matrix,
                         f32vec4(aabb_vertex_base_offsets[vertex_index], 1.0f))));

    return output;
}

[shader("fragment")]
f32vec4 fragment_main(f32vec3 color : f32vec3, f32vec4 pixel: SV_Position) : SV_Target {
    const f32 depth_bufer_depth = Texture2D<f32>::get(push.uses.u_depth_image).Load(u32vec3(pixel.xy, 0)).x;
    if(depth_bufer_depth > pixel.z) { discard; }
    return f32vec4(color, 1.0);
}   
#endif
#include "daxa/daxa.inl"
#include "debug.inl"
#include "../../../shader_library/visibility_buffer.slang"

[[vk::push_constant]] DebugEntityOOBDrawPush push;

static const f32vec3 aabb_vertex_base_offsets[24] = f32vec3[24](
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

struct VertexStageOutput {
    f32vec3 color : f32vec3;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput entity_obb_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const ShaderDebugEntityOOBDraw obb = push.uses.u_globals->debug->entity_oob_draws[instance_index];
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;

    VertexStageOutput output;
    output.color = obb.color;
    output.sv_position = mul(projection_view_matrix,
                         mul(push.uses.u_transforms[obb.transform_index].model_matrix,
                         f32vec4(aabb_vertex_base_offsets[vertex_index], 1.0f)));

    return output;
}

[shader("vertex")]
VertexStageOutput aabb_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const ShaderDebugAABBDraw aabb = push.uses.u_globals->debug->aabb_draws[instance_index];
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;
    const float3 model_position = aabb_vertex_base_offsets[vertex_index] * aabb.extent + aabb.position;

    VertexStageOutput output;
    output.color = aabb.color;
    output.sv_position = mul(projection_view_matrix, f32vec4(model_position, 1.0f));

    return output;
}

[shader("fragment")]
f32vec4 fragment_main(f32vec3 color : f32vec3, f32vec4 pixel: SV_Position) : SV_Target {
    u32 vis_index = 0;
    f32 depth = 0;
    decode_visibility_buffer_payload(tex_rw_u64_table[push.uses.u_visibility_image.index()][pixel.xy], vis_index, depth);
    if(depth > pixel.z) { discard; }
    return f32vec4(color, 1.0);
}   
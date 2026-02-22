#include "daxa/daxa.inl"
#include "debug.inl"

[[vk::push_constant]] DebugAABBDrawPush push;

static const f32vec3 aabb_vertex_base_offsets[24] = f32vec3[24](
    // Bottom rectangle:
    f32vec3(-1.0, -1.0, -1.0), f32vec3(+1.0, -1.0, -1.0),
    f32vec3(-1.0, -1.0, -1.0), f32vec3(-1.0, +1.0, -1.0),
    f32vec3(-1.0, +1.0, -1.0), f32vec3(+1.0, +1.0, -1.0),
    f32vec3(+1.0, -1.0, -1.0), f32vec3(+1.0, +1.0, -1.0),
    // Top rectangle:
    f32vec3(-1.0, -1.0, +1.0), f32vec3(+1.0, -1.0, +1.0),
    f32vec3(-1.0, -1.0, +1.0), f32vec3(-1.0, +1.0, +1.0),
    f32vec3(-1.0, +1.0, +1.0), f32vec3(+1.0, +1.0, +1.0),
    f32vec3(+1.0, -1.0, +1.0), f32vec3(+1.0, +1.0, +1.0),
    // Connecting lines:
    f32vec3(-1.0, -1.0, -1.0), f32vec3(-1.0, -1.0, +1.0),
    f32vec3(+1.0, -1.0, -1.0), f32vec3(+1.0, -1.0, +1.0),
    f32vec3(-1.0, +1.0, -1.0), f32vec3(-1.0, +1.0, +1.0),
    f32vec3(+1.0, +1.0, -1.0), f32vec3(+1.0, +1.0, +1.0)
);

struct VertexStageOutput {
    f32vec3 color : f32vec3;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput aabb_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const ShaderDebugAABBDraw aabb = push.uses.u_globals->debug->aabb_draws[instance_index];
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;
    const f32vec3 model_position = aabb_vertex_base_offsets[vertex_index] * aabb.extent + aabb.position;

    VertexStageOutput output;
    output.color = aabb.color;
    output.sv_position = mul(projection_view_matrix, f32vec4(model_position, 1.0f));

    return output;
}

[shader("vertex")]
VertexStageOutput circle_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const ShaderDebugCircleDraw circle = push.uses.u_globals->debug->circle_draws[instance_index];
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;
    
    const f32 rotation = f32(vertex_index) * (1.0f / (32.0f - 1.0f)) * 2.0f * 3.14f;
    const f32vec3 model_position = circle.position + circle.radius * f32vec3(cos(rotation), sin(rotation), 0.0f);

    VertexStageOutput output;
    output.color = circle.color;
    output.sv_position = mul(projection_view_matrix, f32vec4(model_position, 1.0f));

    return output;
}

[shader("vertex")]
VertexStageOutput line_main(u32 vertex_index: SV_VertexID, u32 instance_index : SV_InstanceID) {
    const ShaderDebugLineDraw line = push.uses.u_globals->debug->line_draws[instance_index];
    const f32mat4x4 projection_view_matrix = push.uses.u_globals->render_as_observer ? push.uses.u_globals->observer_camera.projection_view_matrix : push.uses.u_globals->main_camera.projection_view_matrix;
    const f32vec3 model_position = vertex_index == 0 ? line.start : line.end;

    VertexStageOutput output;
    output.color = line.color;
    output.sv_position = mul(projection_view_matrix, f32vec4(model_position, 1.0f));

    return output;
}

[shader("fragment")]
f32vec4 fragment_main(f32vec3 color : f32vec3, f32vec4 pixel: SV_Position) : SV_Target {
    return f32vec4(color, 1.0);
}   
#include <daxa/daxa.inl>
#include "debug.inl"

DAXA_DECL_PUSH_CONSTANT(DebugDrawPush, push)
const vec3 aabb_vertex_base_offsets[] = vec3[](
    // Bottom rectangle:
    vec3(-1,-1,-1), vec3( 1,-1,-1),
    vec3(-1,-1,-1), vec3(-1, 1,-1),
    vec3(-1, 1,-1), vec3( 1, 1,-1),
    vec3( 1,-1,-1), vec3( 1, 1,-1),
    // Top rectangle:
    vec3(-1,-1, 1), vec3( 1,-1, 1),
    vec3(-1,-1, 1), vec3(-1, 1, 1),
    vec3(-1, 1, 1), vec3( 1, 1, 1),
    vec3( 1,-1, 1), vec3( 1, 1, 1),
    // Connecting lines:
    vec3(-1,-1,-1), vec3(-1,-1, 1),
    vec3( 1,-1,-1), vec3( 1,-1, 1),
    vec3(-1, 1,-1), vec3(-1, 1, 1),
    vec3( 1, 1,-1), vec3( 1, 1, 1)
);

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out vec3 vtf_color;

void main() {
    const uint aabb_idx = gl_InstanceIndex;
    const uint vertex_idx = gl_VertexIndex;

    const ShaderDebugAABBDraw aabb = deref(deref(deref(push.uses.u_globals).debug).aabb_draws + aabb_idx);

    vec4 out_position;
    const vec3 vertex_pos = aabb_vertex_base_offsets[vertex_idx] * 0.5f;
    const vec4 model_position = deref(push.uses.u_transforms[aabb.transforms_handle]).model_matrix * vec4(vertex_pos, 1.0f);
    vtf_color = aabb.color;
    out_position = deref(push.uses.u_globals).camera_projection_view_matrix * vec4(model_position);
    gl_Position = out_position;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec3 vtf_color;

layout(location = 0) out vec4 color;
void main()
{
    const float depth_bufer_depth = texelFetch(daxa_texture2D(push.uses.u_depth_image), ivec2(gl_FragCoord.xy), 0).x;
    if(depth_bufer_depth > gl_FragCoord.z) { discard; }
    color = vec4(vtf_color,1);
}

#endif
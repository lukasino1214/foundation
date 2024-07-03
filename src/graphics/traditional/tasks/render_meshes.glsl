#include "daxa/daxa.inl"
#include "render_meshes.inl"

DAXA_DECL_PUSH_CONSTANT(RenderMeshesPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out daxa_f32vec2 out_uv;

void main() {
    out_uv = deref(push.vertices[gl_VertexIndex]).uv;
    gl_Position = deref(push.uses.u_globals).camera_projection_matrix * deref(push.uses.u_globals).camera_view_matrix * deref(push.transform).model_matrix * daxa_f32vec4(deref(push.vertices[gl_VertexIndex]).position, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in daxa_f32vec2 in_uv;
layout(location = 0) out daxa_f32vec4 out_color;

void main() {
    daxa_f32vec3 color = daxa_f32vec3(0.0);
    daxa_ImageViewId albedo_texture_id = deref(push.material).albedo_texture_id;
    if(albedo_texture_id.value != 0) {
        color = texture(daxa_sampler2D(albedo_texture_id, deref(push.material).albedo_sampler_id), in_uv).rgb;
    }
    daxa_ImageViewId emissive_texture_id = deref(push.material).emissive_texture_id;
    if(emissive_texture_id.value != 0) {
        color += texture(daxa_sampler2D(emissive_texture_id, deref(push.material).emissive_sampler_id), in_uv).rgb;
    }
    
    out_color = daxa_f32vec4(color, 1.0);
}

#endif
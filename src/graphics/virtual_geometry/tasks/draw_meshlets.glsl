#include <daxa/daxa.inl>
#include "draw_meshlets.inl"

DAXA_DECL_PUSH_CONSTANT(DrawMeshletsPush, push)

uint get_micro_index(daxa_BufferPtr(daxa_u32) micro_indices, daxa_u32 index_offset) {
    uint pack_index = index_offset / 4;
    uint index_pack = deref(micro_indices[pack_index]);
    uint in_pack_offset = index_offset % 4;
    uint in_pack_shift = in_pack_offset * 8;
    return (index_pack >> in_pack_shift) & 0xFF;
}

void encode_triangle_id(inout daxa_u32 id, in daxa_u32 meshlet_index, in daxa_u32 triangle_index) {
    id = (meshlet_index << 8) | (triangle_index);
}

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out flat daxa_u32 out_id;

void main() {
    const uint triangle_corner_index = gl_VertexIndex % 3;
    const uint inst_meshlet_index = gl_InstanceIndex;
    const uint triangle_index = gl_VertexIndex / 3;

    MeshletData meshlet_data = deref(push.uses.u_meshlets_data).meshlets[inst_meshlet_index];
    Mesh mesh = deref((push.uses.u_meshes[meshlet_data.mesh_index]));
    Meshlet meshlet = deref(mesh.meshlets[meshlet_data.meshlet_index]);

    if (triangle_index >= meshlet.triangle_count) {
        gl_Position = vec4(10, 10, 10, 1); // yeet
        return;
    }

    daxa_BufferPtr(daxa_u32) micro_index_buffer = mesh.micro_indices;
    const uint micro_index = get_micro_index(micro_index_buffer, meshlet.micro_indices_offset + triangle_index * 3 + triangle_corner_index);
    uint vertex_index = deref(mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index]);
    vertex_index = min(vertex_index, mesh.vertex_count - 1);

    //encode_triangle_id(out_id, inst_meshlet_index, triangle_index);
    out_id = vertex_index;
    
    const vec4 vertex_position = vec4(deref(mesh.vertex_positions[vertex_index]), 1);

    gl_Position = deref(push.uses.u_globals).camera_projection_matrix * deref(push.uses.u_globals).camera_view_matrix * deref(push.uses.u_transforms[meshlet_data.mesh_group_index]).model_matrix * vertex_position;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in flat daxa_u32 in_id;

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

vec3 saturate(in vec3 v) {
    return clamp(v, 0.0, 1.0);
}

daxa_f32vec3 hsv_to_rgb(in daxa_f32vec3 hsv) {
    const daxa_f32vec3 rgb = saturate(abs(mod(hsv.x * 6.0 + daxa_f32vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * mix(daxa_f32vec3(1.0), rgb, hsv.y);
}

layout(location = 0) out daxa_f32vec4 color;

void main() {
    color = daxa_f32vec4(hsv_to_rgb(daxa_f32vec3(float(in_id) * M_GOLDEN_CONJ, 0.875, 0.85)), 1);
    //color = daxa_f32vec4(1);
}

#endif
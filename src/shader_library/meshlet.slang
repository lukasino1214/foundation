#pragma once
#include "glue.inl"
#include "shared.inl"

u32 get_micro_index(u32* micro_indices, u32 index_offset) {
    u32 pack_index = index_offset / 4;
    u32 index_pack = micro_indices[pack_index];
    u32 in_pack_offset = index_offset % 4;
    u32 in_pack_shift = in_pack_offset * 8;
    return (index_pack >> in_pack_shift) & 0xFF;
}

u32 get_vertex_index(Mesh mesh, Meshlet meshlet, u32 triangle_index, u32 offset) {
    daxa_BufferPtr(u32) micro_index_buffer = mesh.micro_indices;
    const uint micro_index = get_micro_index(micro_index_buffer, meshlet.micro_indices_offset + triangle_index * 3 + offset);
    uint vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
    return min(vertex_index, mesh.vertex_count - 1);
}

void encode_triangle_id(inout u32 id, in u32 meshlet_index, in u32 triangle_index) {
    id = (meshlet_index << 8) | (triangle_index);
}

void decode_triangle_id(in u32 id, out u32 meshlet_index, out u32 triangle_index) {
    meshlet_index = id >> 8;
    triangle_index = id & 0xFF;
}
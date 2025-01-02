#pragma once
#include "glue.inl"
#include "shared.inl"
#include "cull_util.slang"

u32 get_micro_index(u32* micro_indices, u32 index_offset) {
    u32 pack_index = index_offset / 4;
    u32 index_pack = micro_indices[pack_index];
    u32 in_pack_offset = index_offset % 4;
    u32 in_pack_shift = in_pack_offset * 8;
    return (index_pack >> in_pack_shift) & 0xFF;
}

u32 get_vertex_index(Mesh mesh, Meshlet meshlet, u32 triangle_index, u32 offset) {
    daxa_BufferPtr(u32) micro_index_buffer = mesh.micro_indices;
    const u32 micro_index = get_micro_index(micro_index_buffer, meshlet.micro_indices_offset + triangle_index * 3 + offset);
    u32 vertex_index = mesh.indirect_vertices[meshlet.indirect_vertex_offset + micro_index];
    return min(vertex_index, mesh.vertex_count - 1);
}

void encode_triangle_id(inout u32 id, in u32 meshlet_index, in u32 triangle_index) {
    id = (meshlet_index << 6) | (triangle_index);
}

void decode_triangle_id(in u32 id, out u32 meshlet_index, out u32 triangle_index) {
    meshlet_index = id >> 6;
    triangle_index = id & 0x3F;
}

#define DECL_GET_SET(TYPE, FIELD)\
    [mutating]\
    func set_##FIELD(TYPE v);\
    func get_##FIELD() -> TYPE;

#define IMPL_GET_SET(TYPE, FIELD)\
    [mutating]\
    func set_##FIELD(TYPE v) { FIELD = v; }\
    func get_##FIELD() -> TYPE { return FIELD; };

interface VertexT {
    DECL_GET_SET(f32vec4, position)
};

interface PrimtiveT {
    DECL_GET_SET(bool, cull_primitive)
};

struct Vertex : VertexT {
    f32vec4 position: SV_Position;
    IMPL_GET_SET(float4, position)
}

struct Primtive : PrimtiveT {
    bool cull_primitive: SV_CullPrimitive;
    IMPL_GET_SET(bool, cull_primitive)
}

struct VisibilityPrimitive : PrimtiveT {
    bool cull_primitive: SV_CullPrimitive;
    IMPL_GET_SET(bool, cull_primitive)
    nointerpolation [[vk::location(0)]] u32 vis_index;
}

void generic_mesh<V: VertexT, P: PrimtiveT>(
    in u32 thread_index, 
    in u32 meshlet_index, 
    in Mesh mesh, 
    in Meshlet meshlet,
    in f32mat4x4 pvm,
    out OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    out OutputVertices<V, MAX_VERTICES_PER_MESHLET> out_vertices,
    out OutputPrimitives<P, MAX_TRIANGLES_PER_MESHLET> out_primitives
) {
    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);
    if(thread_index < meshlet.vertex_count) {
        V vertex;
        vertex.set_position(mul(pvm, f32vec4(mesh.vertex_positions[mesh.indirect_vertices[meshlet.indirect_vertex_offset + thread_index]], 1.0f)));
        out_vertices[thread_index] = vertex;
    }

    if(thread_index < meshlet.triangle_count) {
        u32vec3 indices = u32vec3(
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 0),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 1),
            get_micro_index(mesh.micro_indices, meshlet.micro_indices_offset + thread_index * 3 + 2)
        );

        out_indices[thread_index] = indices;
        
        P primitive;
        primitive.set_cull_primitive(false);
        
        if(P is VisibilityPrimitive) {
            var visibility_primitive = reinterpret<VisibilityPrimitive>(primitive);
            encode_triangle_id(visibility_primitive.vis_index, meshlet_index, thread_index);
            primitive = reinterpret<P>(visibility_primitive);
        }

        out_primitives[thread_index] = primitive;
    }
}

#define SOFTWARE_RASTERIZATION_OPTIMIZATION 2
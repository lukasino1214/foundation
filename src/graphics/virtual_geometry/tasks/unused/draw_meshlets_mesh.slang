#include "daxa/daxa.inl"
#include "draw_meshlets_mesh.inl"
#include "../../../shader_library/vertex_compression.inl"
#include "../../../shader_library/visibility_buffer.slang"
#include "../../../shader_library/meshlet.slang"
#include "../../../shader_library/cull_util.slang"

[[vk::push_constant]] DrawMeshletsMeshWriteCommandPush write_push;
[[vk::push_constant]] DrawMeshletsMeshPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void draw_meshlets_mesh_write_command() {
    const u32 needed_threads = write_push.uses.u_meshlets_data->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 32);
    DispatchIndirectStruct command;
    command.x = 1;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

struct MeshPayload {
    u32 offset;
    u32 count;
};

// groupshared u32 hw_meshlet_count;
// groupshared u32 test[32];
// [shader("amplification")]
// [numthreads(32, 1, 1)]
// void draw_meshlets_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
//     const u32 count = push.uses.u_meshlets_data->count;
//     const u32 current_thread_index = global_thread_index.x;
//     const bool thread_active = current_thread_index < count;
    
//     if(local_thread_index.x == 0) {
//         hw_meshlet_count = 0;
//     }

//     GroupMemoryBarrierWithGroupSync();

//     if(thread_active) {
//         u32 i = 0;
//         InterlockedAdd(hw_meshlet_count, 1, i);
//         test[i] = current_thread_index;
//     }

//     GroupMemoryBarrierWithGroupSync();

//     if(local_thread_index.x == 0) {
//         printf("%u %u\n", current_thread_index, hw_meshlet_count);
//         MeshPayload payload;
//         DispatchMesh(1, 1, 1, payload);
//     }
// }

// struct Vertex {
//     f32vec4 sv_position : SV_Position;
// };

// struct Primtive {
//     nointerpolation [[vk::location(0)]] u32 vis_index;
//     bool cull_primitive : SV_CullPrimitive;
// };

// #define MAX_VERTICES_PER_THREAD ((MAX_VERTICES_PER_MESHLET + 32 - 1) / 32)
// #define MAX_TRIANGLES_PER_THREAD ((MAX_TRIANGLES_PER_MESHLET + 32 - 1) / 32)

// // groupshared f32vec4 clip_vertex_positions[MAX_VERTICES_PER_MESHLET];
// groupshared Mesh mesh;
// groupshared Meshlet meshlet;
// groupshared u32 meshlet_index;
// groupshared f32mat4x4 pvm;

// [outputtopology("triangle")]
// [shader("mesh")]
// [numthreads(1, 1, 1)]
// void draw_meshlets_mesh(
//     in payload MeshPayload payload,
//     u32vec3 local_thread_index : SV_GroupThreadID,
//     u32vec3 group_index : SV_GroupID,
//     u32vec3 global_thread_index : SV_DispatchThreadID,
//     OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
//     OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
//     OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {

//     printf("draw_meshlets_mesh: group_index: %u\n", payload.offset + group_index.x);
// }

// [shader("fragment")]
// void draw_meshlets_frag(in Vertex vertex, in Primtive primitive) {
//     AtomicMaxU64(tex_rw_u64_table[push.uses.u_visibility_image.index()][u32vec2(vertex.sv_position.xy)], encode_visibility_buffer_payload(primitive.vis_index, vertex.sv_position.z));
// }   

// groupshared u32 hw_meshlet_count;
// [shader("amplification")]
// [numthreads(1, 1, 1)]
// void draw_meshlets_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
//     const u32 count = push.uses.u_meshlets_data->count;
    
//     if(local_thread_index.x == 0) {
//         printf("%u\n", count);
//         hw_meshlet_count = 0;
//         u32 i = 0;
//         InterlockedAdd(hw_meshlet_count, 1, i);
//         hw_meshlet_count = i;
//     }
        
//     MeshPayload payload;
//     DispatchMesh(hw_meshlet_count, 1, 1, payload);
// }

// struct Vertex {
//     f32vec4 sv_position : SV_Position;
// };

// struct Primtive {
//     nointerpolation [[vk::location(0)]] u32 vis_index;
//     bool cull_primitive : SV_CullPrimitive;
// };

// [outputtopology("triangle")]
// [shader("mesh")]
// [numthreads(1, 1, 1)]
// void draw_meshlets_mesh(
//     in payload MeshPayload payload,
//     u32vec3 local_thread_index : SV_GroupThreadID,
//     u32vec3 group_index : SV_GroupID,
//     u32vec3 global_thread_index : SV_DispatchThreadID,
//     OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
//     OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
//     OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {}

// [shader("fragment")]
// void draw_meshlets_frag(in Vertex vertex, in Primtive primitive) {
//     AtomicMaxU64(tex_rw_u64_table[push.uses.u_visibility_image.index()][u32vec2(vertex.sv_position.xy)], encode_visibility_buffer_payload(primitive.vis_index, vertex.sv_position.z));
// }   


groupshared u32 hw_meshlet_count;
[shader("amplification")]
[numthreads(32, 1, 1)]
void draw_meshlets_task(u32vec3 local_thread_index : SV_GroupThreadID, u32vec3 global_thread_index : SV_DispatchThreadID, u32vec3 group_index : SV_GroupID) {
    const u32 count = push.uses.u_meshlets_data->count; // reading from pointer in push constant
    
    if(local_thread_index.x == 0) {
        printf("%u\n", count); // commenting this make validation layer error disappear

        // also commenting makes validation layer error disappear.
        // printf or atomic code below has to be commented in other to not crash
        u32 i = 0;
        InterlockedAdd(hw_meshlet_count, 1, i);
        hw_meshlet_count = i;
    }

    GroupMemoryBarrierWithGroupSync();
    MeshPayload payload;
    DispatchMesh(1, 1, 1, payload);
}

struct Vertex {
    f32vec4 sv_position : SV_Position;
};

struct Primtive {
    nointerpolation [[vk::location(0)]] u32 vis_index;
    bool cull_primitive : SV_CullPrimitive;
};

[outputtopology("triangle")]
[shader("mesh")]
[numthreads(1, 1, 1)]
void draw_meshlets_mesh(
    in payload MeshPayload payload,
    u32vec3 local_thread_index : SV_GroupThreadID,
    u32vec3 group_index : SV_GroupID,
    u32vec3 global_thread_index : SV_DispatchThreadID,
    OutputIndices<u32vec3, MAX_TRIANGLES_PER_MESHLET> out_indices,
    OutputVertices<Vertex, MAX_VERTICES_PER_MESHLET> out_vertices,
    OutputPrimitives<Primtive, MAX_TRIANGLES_PER_MESHLET> out_primitives) {}

[shader("fragment")]
void draw_meshlets_frag(in Vertex vertex, in Primtive primitive) {}   
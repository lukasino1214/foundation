#include "daxa/daxa.inl"
#include "triangle.inl"

struct Vertex {
    f32vec2 pos;
    f32vec3 color;
};

struct VertexStageOutput {
    Vertex vertex : Vertex;
    f32vec4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput vertex_main(u32 index: SV_VertexID) {
    Vertex data[3] = {
        { { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } }
    };

    VertexStageOutput output;
    output.vertex = data[index];
    output.sv_position = f32vec4(data[index].pos, 0.0, 1.0);
    return output;
}

[shader("fragment")]
f32vec4 fragment_main(Vertex vertex : Vertex) : SV_Target {
    return daxa_f32vec4(vertex.color, 1.0f);
}   
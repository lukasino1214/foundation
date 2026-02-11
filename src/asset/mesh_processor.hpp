#pragma once

#include "asset_manifests.hpp"

namespace foundation {
    struct MeshProcessor {
        static auto process_mesh(const LoadedMesh& loaded_mesh) -> LoadedMesh;

    private:
        struct GenerateIndexBufferInfo {
            std::vector<f32vec3> unindexed_positions = {};
            std::vector<f32vec3> unindexed_normals = {};
            std::vector<f32vec2> unindexed_uvs = {};
        };

        struct ProcessedIndexBufferInfo {
            std::vector<f32vec3> positions = {};
            std::vector<f32vec3> normals = {};
            std::vector<f32vec2> uvs = {};
            std::vector<u32> indices = {};
        };

        static auto generate_index_buffer(const GenerateIndexBufferInfo& info) -> ProcessedIndexBufferInfo;
        static auto optimize_vertex_cache(const std::vector<u32>& index_buffer, u32 vertex_count) -> std::vector<u32>;
        static auto optimize_vertex_fetch(const ProcessedIndexBufferInfo& info) -> ProcessedIndexBufferInfo;
    };
}
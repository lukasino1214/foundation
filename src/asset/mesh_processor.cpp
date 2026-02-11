#include "mesh_processor.hpp"

#include <meshoptimizer.h>

namespace foundation {
    auto MeshProcessor::process_mesh(const LoadedMesh& loaded_mesh) -> LoadedMesh {
        LoadedMesh processed_mesh = {};
        processed_mesh.name = loaded_mesh.name;
        if(loaded_mesh.indices.empty()) {
            ProcessedIndexBufferInfo ret = generate_index_buffer(GenerateIndexBufferInfo {
                .unindexed_positions = loaded_mesh.positions,
                .unindexed_normals = loaded_mesh.normals,
                .unindexed_uvs = loaded_mesh.uvs,
            });

            processed_mesh.positions = ret.positions;
            processed_mesh.normals = ret.normals;
            processed_mesh.uvs = ret.uvs;
            processed_mesh.indices = ret.indices;
        }

        {
            ProcessedIndexBufferInfo ret = optimize_vertex_fetch(ProcessedIndexBufferInfo {
                .positions = loaded_mesh.positions,
                .normals = loaded_mesh.normals,
                .uvs = loaded_mesh.uvs,
                .indices = loaded_mesh.indices,
            });

            processed_mesh.positions = ret.positions;
            processed_mesh.normals = ret.normals;
            processed_mesh.uvs = ret.uvs;
            processed_mesh.indices = ret.indices;
        }

        processed_mesh.indices = optimize_vertex_cache(processed_mesh.indices, s_cast<u32>(processed_mesh.positions.size()));
        return processed_mesh;
    }

    auto MeshProcessor::generate_index_buffer(const GenerateIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        PROFILE_SCOPE;

        std::vector<meshopt_Stream> streams = {};
        streams.push_back(meshopt_Stream { .data = info.unindexed_positions.data(), .size = info.unindexed_positions.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_normals.data(), .size = info.unindexed_normals.size(), .stride = sizeof(f32vec3) });
        streams.push_back(meshopt_Stream { .data = info.unindexed_uvs.data(), .size = info.unindexed_uvs.size(), .stride = sizeof(f32vec2) });
        
        usize index_count = info.unindexed_positions.size();
        usize unindexed_vertex_count = info.unindexed_positions.size();
        
        std::vector<u32> remap_table(unindexed_vertex_count);
        usize unique_vertex_count = meshopt_generateVertexRemapMulti(remap_table.data(), nullptr, index_count, unindexed_vertex_count, streams.data(), streams.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.indices.resize(index_count);

        meshopt_remapVertexBuffer(ret.positions.data(), info.unindexed_positions.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), info.unindexed_normals.data(), unindexed_vertex_count, sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), info.unindexed_uvs.data(), unindexed_vertex_count, sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.indices.data(), nullptr, index_count, remap_table.data());

        return ret;
    }

    auto MeshProcessor::optimize_vertex_cache(const std::vector<u32>& index_buffer, u32 vertex_count) -> std::vector<u32> {
        PROFILE_SCOPE;

        std::vector<u32> optimized_index_buffer(index_buffer.size());
        meshopt_optimizeVertexCache(optimized_index_buffer.data(), index_buffer.data(), index_buffer.size(), vertex_count);
        return optimized_index_buffer;
    }

    auto MeshProcessor::optimize_vertex_fetch(const ProcessedIndexBufferInfo& info) -> ProcessedIndexBufferInfo {
        PROFILE_SCOPE;

        std::vector<u32> remap_table(info.positions.size());
        usize unique_vertex_count = meshopt_optimizeVertexFetchRemap(remap_table.data(), info.indices.data(), info.indices.size(), info.positions.size());
        
        ProcessedIndexBufferInfo ret = {};
        ret.positions.resize(unique_vertex_count);
        ret.normals.resize(unique_vertex_count);
        ret.uvs.resize(unique_vertex_count);
        ret.indices.resize(info.indices.size());

        meshopt_remapVertexBuffer(ret.positions.data(), info.positions.data(), info.positions.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.normals.data(), info.normals.data(), info.normals.size(), sizeof(f32vec3), remap_table.data());
        meshopt_remapVertexBuffer(ret.uvs.data(), info.uvs.data(), info.uvs.size(), sizeof(f32vec2), remap_table.data());
        meshopt_remapIndexBuffer(ret.indices.data(), info.indices.data(), info.indices.size(), remap_table.data());

        return ret;
    }
}
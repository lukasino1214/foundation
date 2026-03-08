#include "mesh_processor.hpp"

#include <meshoptimizer.h>
#include <shader_library/compression.inl>

namespace foundation {
    auto MeshProcessor::process_mesh(const LoadedMesh& loaded_mesh) -> ProcessedMesh {
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

        constexpr usize MAX_VERTICES = MAX_VERTICES_PER_MESHLET;
        constexpr usize MAX_TRIANGLES = MAX_TRIANGLES_PER_MESHLET;
        constexpr f32 CONE_WEIGHT = 1.0f;

        usize max_meshlets = meshopt_buildMeshletsBound(processed_mesh.indices.size(), MAX_VERTICES, MAX_TRIANGLES);
        processed_mesh.meshlets.resize(max_meshlets);
        processed_mesh.indirect_vertices.resize(max_meshlets * MAX_VERTICES);
        processed_mesh.micro_indices.resize(max_meshlets * MAX_TRIANGLES * 3);

        usize meshlet_count = meshopt_buildMeshlets(
            r_cast<meshopt_Meshlet*>(processed_mesh.meshlets.data()),
            processed_mesh.indirect_vertices.data(),
            processed_mesh.micro_indices.data(),
            processed_mesh.indices.data(),
            processed_mesh.indices.size(),
            r_cast<const f32*>(processed_mesh.positions.data()),
            processed_mesh.positions.size(),
            sizeof(f32vec3),
            MAX_VERTICES,
            MAX_TRIANGLES,
            CONE_WEIGHT
        );

        const Meshlet& last = processed_mesh.meshlets[meshlet_count - 1];
        processed_mesh.indirect_vertices.resize(last.indirect_vertex_offset + last.vertex_count);
        processed_mesh.micro_indices.resize(last.micro_indices_offset + ((last.triangle_count * 3u + 3u) & ~3u));
        processed_mesh.meshlets.resize(meshlet_count);

        for(const Meshlet& meshlet : processed_mesh.meshlets) {
            meshopt_optimizeMeshlet(
                &processed_mesh.indirect_vertices[meshlet.indirect_vertex_offset], 
                &processed_mesh.micro_indices[meshlet.micro_indices_offset], 
                meshlet.triangle_count, 
                meshlet.vertex_count
            );
        }

        processed_mesh.bounding_spheres.resize(meshlet_count);
        processed_mesh.aabbs.resize(meshlet_count);

        f32vec3 mesh_aabb_max = glm::vec3{std::numeric_limits<f32>::lowest()};
        f32vec3 mesh_aabb_min = glm::vec3{std::numeric_limits<f32>::max()};

        for(usize meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index) {
            const auto& meshlet = processed_mesh.meshlets[meshlet_index];

            meshopt_Bounds raw_bounds = meshopt_computeMeshletBounds(
                &processed_mesh.indirect_vertices[meshlet.indirect_vertex_offset],
                &processed_mesh.micro_indices[meshlet.micro_indices_offset],
                meshlet.triangle_count,
                r_cast<const f32*>(processed_mesh.positions.data()),
                processed_mesh.positions.size(),
                sizeof(glm::vec3)
            );

            processed_mesh.bounding_spheres[meshlet_index] = BoundingSphere {
                .center = { raw_bounds.center[0], raw_bounds.center[1], raw_bounds.center[2]},
                .radius = raw_bounds.radius
            };

            glm::vec3 min_pos = processed_mesh.positions[processed_mesh.indirect_vertices[meshlet.indirect_vertex_offset]];
            glm::vec3 max_pos = processed_mesh.positions[processed_mesh.indirect_vertices[meshlet.indirect_vertex_offset]];

            for (u32 i = 0; i < meshlet.vertex_count; i++) {
                glm::vec3 pos = processed_mesh.positions[processed_mesh.indirect_vertices[meshlet.indirect_vertex_offset + i]];
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }

            mesh_aabb_min = glm::min(mesh_aabb_min, min_pos);
            mesh_aabb_max = glm::max(mesh_aabb_max, max_pos);

            processed_mesh.aabbs[meshlet_index].center = (max_pos + min_pos) * 0.5f;
            processed_mesh.aabbs[meshlet_index].extent =  (max_pos - min_pos) * 0.5f;
        }

        processed_mesh.mesh_aabb = {
            .center = (mesh_aabb_max + mesh_aabb_min) * 0.5f,
            .extent =  (mesh_aabb_max - mesh_aabb_min) * 0.5f,
        };

        std::vector<u32> compressed_normals = {};
        compressed_normals.reserve(processed_mesh.normals.size());

        for(const glm::vec3& normal : processed_mesh.normals) {
            compressed_normals.push_back(encode_normal(normal));
        }

        std::vector<u32> compressed_uvs = {};
        compressed_uvs.reserve(processed_mesh.uvs.size());

        for(const glm::vec2& uv : processed_mesh.uvs) {
            compressed_uvs.push_back(encode_uv(uv));
        }

        return {
            .positions = std::move(processed_mesh.positions),
            .normals = std::move(compressed_normals),
            .uvs = std::move(compressed_uvs),
            .indices = std::move(processed_mesh.indices),
            .meshlets = std::move(processed_mesh.meshlets),
            .indirect_vertices = std::move(processed_mesh.indirect_vertices),
            .micro_indices = std::move(processed_mesh.micro_indices),
            .bounding_spheres = std::move(processed_mesh.bounding_spheres),
            .aabbs = std::move(processed_mesh.aabbs),
            .mesh_aabb = processed_mesh.mesh_aabb,
            .name = processed_mesh.name,
        };
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
#pragma once

#include <ecs/entity.hpp>

namespace foundation {
    struct GPUContext;

    struct IScene {
        std::vector<u32> node_indices = {};
        std::string name = {};
    };

    struct Node {
        std::optional<u32> mesh_group_index = {};

        glm::mat4 transform = {1.0f};

        std::vector<u32> children = {};
        std::string name = {};
    };

    struct Mesh {
        std::optional<u32> material_index = {};
        
        daxa::BufferId mesh_buffer = {};
        daxa::BufferId index_buffer = {};
        MeshGeometryData mesh = {};
        u32 vertex_count = {};
        u32 index_count = {};
    };

    struct MeshGroup {
        std::vector<Mesh> meshes = {};
        std::string name = {};
    };

    struct Material {

    };

    struct Image {
        daxa::ImageId image;
        daxa::SamplerId sampler;
        std::string name = {};
    };

    struct ModelData {
        std::filesystem::path file_path = {};
        std::vector<IScene> scenes = {};
        std::vector<Node> nodes = {};
        std::vector<MeshGroup> mesh_groups = {};
        std::vector<Image> images = {};
        std::vector<Material> materials = {};
        daxa::BufferId materials_buffer = {};
    };

    struct AssetManager {
        auto load(const std::filesystem::path& file_path, GPUContext* gpu_context, Scene* scene, Entity root_entity) -> ModelData;
    };
}
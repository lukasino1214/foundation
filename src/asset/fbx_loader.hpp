#pragma once

#include "asset_manifests.hpp"

namespace foundation {
    struct FBXLoader {
        static auto load(const std::filesystem::path& file_path, u32 asset_manifest_index) -> LoadedAsset;
    };
}
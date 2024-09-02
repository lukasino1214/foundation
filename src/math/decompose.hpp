#pragma once

#include <pch.hpp>

namespace foundation {
    namespace math {
        auto decompose_transform(const glm::mat4 &transform, glm::vec3 &translation, glm::vec3 &rotation, glm::vec3 &scale) -> bool;
    }
}
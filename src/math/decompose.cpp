#include "decompose.hpp"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace foundation {
    namespace math {
        fn decompose_transform(const glm::mat4 &transform, glm::vec3 &translation, glm::vec3 &rotation, glm::vec3 &scale) -> bool {
            glm::mat4 local_matrix{transform};

            if (glm::epsilonEqual(local_matrix[3][3], s_cast<f32>(0), glm::epsilon<f32>())) {
                return false;
            }

            if (glm::epsilonNotEqual(local_matrix[0][3], s_cast<f32>(0), glm::epsilon<f32>()) || 
                glm::epsilonNotEqual(local_matrix[1][3], s_cast<f32>(0), glm::epsilon<f32>()) || 
                glm::epsilonNotEqual(local_matrix[2][3], s_cast<f32>(0), glm::epsilon<f32>())) {
                local_matrix[0][3] = local_matrix[1][3] = local_matrix[2][3] = s_cast<f32>(0);
                local_matrix[3][3] = s_cast<f32>(1);
            }

            translation = glm::vec3(local_matrix[3]);
            local_matrix[3] = glm::vec4(0, 0, 0, local_matrix[3].w);

            glm::vec3 row[3];

            for (i32 i = 0; i < 3; i++) {
                for (i32 j = 0; j < 3; j++) {
                    row[i][j] = local_matrix[i][j];
                }
            }

            scale.x = length(row[0]);
            row[0] = glm::detail::scale(row[0], s_cast<f32>(1));
            scale.y = length(row[1]);
            row[1] = glm::detail::scale(row[1], s_cast<f32>(1));
            scale.z = length(row[2]);
            row[2] = glm::detail::scale(row[2], s_cast<f32>(1));

            rotation.y = asin(-row[0][2]);
            if (cos(rotation.y) != 0) {
                rotation.x = atan2(row[1][2], row[2][2]);
                rotation.z = atan2(row[0][1], row[0][0]);
            } else {
                rotation.x = atan2(-row[2][0], row[1][1]);
                rotation.z = 0;
            }

            rotation = glm::degrees(rotation);

            return true;
        }
    }
}
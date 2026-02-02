#include "decompose.hpp"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace foundation {
    namespace math {
        auto decompose_transform(const glm::mat4 &transform, glm::vec3 &translation, glm::quat &rotation, glm::vec3 &scale) -> bool {
            // glm::mat4 local_matrix{transform};

            // if (glm::epsilonEqual(local_matrix[3][3], s_cast<f32>(0), glm::epsilon<f32>())) {
            //     return false;
            // }

            // if (glm::epsilonNotEqual(local_matrix[0][3], s_cast<f32>(0), glm::epsilon<f32>()) || 
            //     glm::epsilonNotEqual(local_matrix[1][3], s_cast<f32>(0), glm::epsilon<f32>()) || 
            //     glm::epsilonNotEqual(local_matrix[2][3], s_cast<f32>(0), glm::epsilon<f32>())) {
            //     local_matrix[0][3] = local_matrix[1][3] = local_matrix[2][3] = s_cast<f32>(0);
            //     local_matrix[3][3] = s_cast<f32>(1);
            // }

            // translation = glm::vec3(local_matrix[3]);
            // local_matrix[3] = glm::vec4(0, 0, 0, local_matrix[3].w);

            // glm::vec3 row[3];

            // for (i32 i = 0; i < 3; i++) {
            //     for (i32 j = 0; j < 3; j++) {
            //         row[i][j] = local_matrix[i][j];
            //     }
            // }

            // scale.x = length(row[0]);
            // row[0] = glm::detail::scale(row[0], s_cast<f32>(1));
            // scale.y = length(row[1]);
            // row[1] = glm::detail::scale(row[1], s_cast<f32>(1));
            // scale.z = length(row[2]);
            // row[2] = glm::detail::scale(row[2], s_cast<f32>(1));

            // glm::mat3 rotation_matrix{row[0], row[1], row[2]};
            // rotation = glm::quat_cast(rotation_matrix);

            // return true;

            glm::mat4 local_matrix{transform};
            translation = local_matrix[3];
            local_matrix[3] = glm::vec4(0.f, 0.f, 0.f, local_matrix[3][3]);

            // Extract the scale. We calculate the euclidean length of the columns.
            // We then construct a vector with those lengths.
            scale = glm::vec3(
                length(local_matrix[0]),
                length(local_matrix[1]),
                length(local_matrix[2])
            );

            // Remove the scaling from the matrix, leaving only the rotation.
            // matrix is now the rotation matrix.
            local_matrix[0] /= scale.x;
            local_matrix[1] /= scale.y;
            local_matrix[2] /= scale.z;

            // Construct the quaternion. This algo is copied from here:
            // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/christian.htm.
            // glTF orders the components as x,y,z,w
            rotation = glm::quat(
                max(.0f, 1.f + local_matrix[0][0] + local_matrix[1][1] + local_matrix[2][2]),
                max(.0f, 1.f + local_matrix[0][0] - local_matrix[1][1] - local_matrix[2][2]),
                max(.0f, 1.f - local_matrix[0][0] + local_matrix[1][1] - local_matrix[2][2]),
                max(.0f, 1.f - local_matrix[0][0] - local_matrix[1][1] + local_matrix[2][2])
            );
            rotation.x = static_cast<float>(sqrt(static_cast<double>(rotation.x))) / 2;
            rotation.y = static_cast<float>(sqrt(static_cast<double>(rotation.y))) / 2;
            rotation.z = static_cast<float>(sqrt(static_cast<double>(rotation.z))) / 2;
            rotation.w = static_cast<float>(sqrt(static_cast<double>(rotation.w))) / 2;

            rotation.x = std::copysignf(rotation.x, local_matrix[1][2] - local_matrix[2][1]);
            rotation.y = std::copysignf(rotation.y, local_matrix[2][0] - local_matrix[0][2]);
            rotation.z = std::copysignf(rotation.z, local_matrix[0][1] - local_matrix[1][0]);

            return true;
        }
    }
}
#pragma once

#include "scene/ISceneNode.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace avalang {
namespace ui {
namespace scene {

inline glm::mat4 Transform::ToMatrix() const {
    glm::mat4 mat = glm::identity<glm::mat4>();

    mat = glm::translate(mat, glm::vec3(position, 0.0f));

    mat = glm::rotate(mat, rotation, glm::vec3(0.0f, 0.0f, 1.0f));

    mat = glm::scale(mat, glm::vec3(scale, 1.0f));

    return mat;
}

}
}
}
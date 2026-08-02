#ifndef AVA_UI_SCENE_TRANSFORM_H
#define AVA_UI_SCENE_TRANSFORM_H

#include "scene/ISceneNode.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace avalang {
namespace ui {
namespace scene {

inline glm::mat4 Transform::ToMatrix() const {
    glm::mat4 mat = glm::identity<glm::mat4>();
    
    // Translate
    mat = glm::translate(mat, glm::vec3(position, 0.0f));
    
    // Rotate around Z axis
    mat = glm::rotate(mat, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Scale
    mat = glm::scale(mat, glm::vec3(scale, 1.0f));
    
    return mat;
}

} // namespace scene
} // namespace ui
} // namespace avalang

#endif // AVA_UI_SCENE_TRANSFORM_H

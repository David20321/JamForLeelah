#pragma once
#ifndef INTERNAL_SEPARABLE_TRANSFORM_HPP
#define INTERNAL_SEPARABLE_TRANSFORM_HPP

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct SeparableTransform {
    glm::quat rotation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::mat4 GetCombination();
    SeparableTransform();
};

#endif
#include "internal/separable_transform.h"
#include "glm/glm.hpp"

SeparableTransform::SeparableTransform() :scale(1.0f) {}

glm::mat4 SeparableTransform::GetCombination() {
    glm::mat4 mat;
    for(int i=0; i<3; ++i){
        mat[i][i] = scale[i];
    }
    for(int i=0; i<3; ++i){
        mat[i] = rotation * mat[i];
    }
    for(int i=0; i<3; ++i){
        mat[3][i] += translation[i];
    }
    return mat;
}

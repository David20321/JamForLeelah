#pragma once
#ifndef INTERNAL_GEOMETRY_H
#define INTERNAL_GEOMETRY_H

#include "glm/glm.hpp"

glm::vec3 ClosestPointOnSegment( const glm::vec3& point, 
                                 const glm::vec3& segment_start, 
                                 const glm::vec3& segment_end);

#endif
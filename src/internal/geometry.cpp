#include "internal/geometry.h"
#include "glm/gtx/norm.hpp"

using namespace glm;

// Adapted from http://paulbourke.net/geometry/pointlineplane/source.c
vec3 ClosestPointOnSegment( const vec3& point, const vec3& segment_start, 
                            const vec3& segment_end) 
{
    vec3 segment_start_to_point = point - segment_start;
    vec3 segment_start_to_end   = segment_end - segment_start;
    float t = dot(segment_start_to_point, segment_start_to_end) / 
        distance2(segment_end, segment_start);
    t = clamp(t,0.0f,1.0f);
    return segment_start + t * segment_start_to_end;
}
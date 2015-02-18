#pragma once
#ifndef PLATFORM_SDL_DEBUG_DRAW_H
#define PLATFORM_SDL_DEBUG_DRAW_H

#include "glm/fwd.hpp"

enum DebugDrawLifetime {
    kUpdate,
    kDraw,
    kPersistent
};

struct DebugDrawCommon {
    DebugDrawLifetime lifetime;
    int lifetime_int;
};

struct DebugDrawLines {
    int shader;
    int vbo;
    static const int kMaxLines = 10000;
    static const int kElementsPerPoint = 7; // Interleaved 3V4C
    DebugDrawCommon* common;
    float* draw_data;
    int num_lines;
    int AllocMemory(void* memory);
    bool Add(const glm::vec3& start, const glm::vec3& end, 
             const glm::vec4& color, DebugDrawLifetime lifetime, int lifetime_int);
    void Draw(const glm::mat4& proj_view_mat);
    void Update();
};

void DrawBoundingBox(DebugDrawLines* lines, const glm::mat4& mat, glm::vec3 bb[]);

#endif
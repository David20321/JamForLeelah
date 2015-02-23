#pragma once
#ifndef GAME_NAV_MESH_H
#define GAME_NAV_MESH_H

#include "glm/glm.hpp"
#include <stdint.h>

class StackAllocator;
struct GraphicsContext;

class NavMesh {
public:
    static const int kMaxNavMeshVerts = 10000;
    static const int kMaxNavMeshTris = 10000;
    int num_verts;
    glm::vec3 verts[kMaxNavMeshVerts];
    int num_indices;
    uint32_t indices[kMaxNavMeshTris*3];
    int vert_vbo;
    int index_vbo;
    int shader;
    int tri_neighbors[kMaxNavMeshTris*3];

    void CalcNeighbors(StackAllocator* stack_allocator);
    void Draw(GraphicsContext* graphics_context, const glm::mat4& proj_view_mat);
    int ClosestTriToPoint(const glm::vec3& pos);
};

class NavMeshWalker {
public:
    int tri;
};

#endif
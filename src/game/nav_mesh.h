#pragma once
#ifndef GAME_NAV_MESH_H
#define GAME_NAV_MESH_H

#include "glm/glm.hpp"
#include <cstdint>

class StackAllocator;

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
    void Draw(const glm::mat4& proj_view_mat);
};

class NavMeshWalker {
public:
    int tri;
    glm::vec3 bary_pos; //barycentric position
    glm::vec3 GetWorldPos(NavMesh* nav_mesh);
    void ApplyWorldSpaceTranslation(NavMesh* nav_mesh, glm::vec3 translation);
    glm::vec3 GetBaryPos(NavMesh* nav_mesh, glm::vec3 pos);
};

#endif
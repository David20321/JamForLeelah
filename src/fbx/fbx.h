#pragma once
#ifndef FBX_HPP
#define FBX_HPP

#include <stdint.h>
#include "glm/fwd.hpp"

struct Mesh {
    static const int kMaxWeightsPerVert = 4;

    int num_verts;
    float* vert_coords; // 3 per vert
    float* vert_bone_weights; // 4 per vert
    int* vert_bone_indices; // 4 per vert
    int num_tris;
    unsigned* tri_indices; // 3 per tri
    float* tri_uvs; // 6 per tri
    float* tri_normals; // 9 per tri

    // To attach to a skeleton
    int num_bones;
    uint64_t* bone_ids;
    float* bind_matrices;

    void Dispose();
    ~Mesh();
};

struct Bone {
    float transform[16]; // matrix 4x4
    float size;
    int parent;
    uint64_t bone_id; // To attach to a mesh
    static const int kMaxBoneNameSize = 256;
    char name[kMaxBoneNameSize]; 
};

struct Animation {
    float* transforms; // Matrix for each bone for each frame
    int num_frames;

    void Dispose();
    ~Animation();
};

struct Skeleton {
    int num_bones;
    Bone* bones;
    int num_animations;
    Animation *animations;

    void Dispose();
    ~Skeleton();
};

struct FBXParseScene {
    int num_mesh;
    Mesh* meshes;
    int num_skeleton;
    Skeleton* skeletons;

    void Dispose();
    ~FBXParseScene();
};

void ParseFBXFromRAM(FBXParseScene* scene, void* file_memory, int file_size, const char** specific_names, int num_names);
void PrintFBXInfoFromRAM(void* file_memory, int file_size);
void AttachMeshToSkeleton(Mesh* mesh, Skeleton* skeleton);
void GetBoundingBox(const Mesh* mesh, glm::vec3* bounding_box);


#endif
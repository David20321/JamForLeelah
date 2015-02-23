#pragma once
#ifndef PLATFORM_SDL_BLENDER_FILE_IO_HPP
#define PLATFORM_SDL_BLENDER_FILE_IO_HPP

#include "glm/fwd.hpp"
#include "SDL_stdinc.h"

class StackAllocator;

class ParseMesh {
public:
    struct Animation {
        int num_frames;
        int anim_transform_start;
        int first_frame;
    };
    int num_vert;
    enum {
        kFloatsPerVert_Skinned = 3+2+3+4+4, //Vert data, 3v 2uv 3n 4bone_index 4bone_weight
        kFloatsPerVert_Unskinned = 3+2+3 //Vert data, 3v 2uv 3n
    };
    float* vert;
    int num_index;
    Uint32* indices;
    int num_bones;
    glm::mat4* rest_mats;
    glm::mat4* inverse_rest_mats;
    int* bone_parents;
    int num_animations;
    Animation* animations;
    glm::mat4* anim_transforms;
    void Dispose();
    ~ParseMesh();
};

void ParseTestFile(const char* path, ParseMesh* mesh_final, StackAllocator* stack_alloc);
#endif
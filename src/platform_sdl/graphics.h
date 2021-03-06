#pragma once
#ifndef PLATFORM_SDL_GRAPHICS_HPP
#define PLATFORM_SDL_GRAPHICS_HPP

#include <SDL.h>
#include "glm/glm.hpp"

#ifdef EMSCRIPTEN
#define USE_OPENGLES
#endif

class FileLoadThreadData;

//TODO: these should all be in a config or something
static const int kMSAA = 4;
static const float kMaxAnisotropy = 4.0f;

static const char* shader_uniform_names[] = {
    "mv_mat",
    "proj_mat",
    "world_mat",
    "norm_mat",
    "texture_id",
    "num_lights",
    "light_pos",
    "light_color",
    "light_type",
    "lamp_tex_id",
    "fog_color",
    "color",
    "time",
    "bone_matrices"
};

struct Shader {
    enum UniformName {
        kModelviewMat4,
        kProjectionMat4,
        kWorldMat4,
        kNormalMat3,
        kTextureID,
        kNumLights,
        kLightPos,
        kLightColor,
        kLightType,
        kLampTexID,
        kFogColor,
        kColor,
        kTime,
        kBoneMatrices,
        kNumUniformNames
    };

    int gl_id;
    int uniforms[kNumUniformNames];
};

struct GraphicsContext {
    static const int kMaxShaders = 10;
    int num_shaders;
    Shader shaders[kMaxShaders];
    int screen_dims[2];
    SDL_Window* window;
    SDL_GLContext gl_context;
};

void InitGraphicsContext(GraphicsContext *graphics_context);
void InitGraphicsData(int *triangle_vbo, int *index_vbo);
int LoadImage(const char* path, FileLoadThreadData* file_load_data);
int CreateShader(int type, const char *src);
int CreateProgram(const int shaders[], int num_shaders);

enum VBO_Type {
    kArrayVBO,
    kElementVBO
};
enum VBO_Hint {
    kStaticVBO,
    kDynamicVBO,
    kStreamVBO
};

int CreateVBO(VBO_Type type, VBO_Hint hint, void* data, int num_data_elements);

void CheckGLError(const char *file, int line);
#ifdef _DEBUG
#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)
#else
#define CHECK_GL_ERROR()
#endif

#endif
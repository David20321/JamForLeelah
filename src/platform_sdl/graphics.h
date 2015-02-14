#pragma once
#ifndef PLATFORM_SDL_GRAPHICS_HPP
#define PLATFORM_SDL_GRAPHICS_HPP

#include <SDL.h>
#include "glm/glm.hpp"

class FileLoadThreadData;

struct GraphicsContext {
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

enum VBO_Setup {
    kSimple_4V, // 4 vert
    kInterleave_3V2T3N, // 3 vert, 2 tex coord, 3 normal
    kInterleave_3V2T3N4I4W // 3 vert, 2 tex coord, 3 normal, 4 bone index, 4 bone weight
};

struct Drawable {
    int texture_id;
    int vert_vbo;
    int index_vbo;
    int num_indices;
    int shader_id;
    VBO_Setup vbo_layout;
    glm::mat4 transform;
};

int CreateVBO(VBO_Type type, VBO_Hint hint, void* data, int num_data_elements);

void CheckGLError(const char *file, int line);
#ifdef _DEBUG
#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)
#else
#define CHECK_GL_ERROR()
#endif

#endif
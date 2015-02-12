#include "platform_sdl/graphics.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/profiler.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <cstring>
#include <cstdlib>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//TODO: these should all be in a config or something
static const int kMSAA = 4;
static const float kMaxAnisotropy = 4.0f;

using namespace glm;

void CheckGLError(const char *file, int line) {
    GLenum err;
    err = glGetError();
    if (err != GL_NO_ERROR)    {
        const char* err_str;
        switch(err){
        case GL_NO_ERROR: err_str = "GL_NO_ERROR"; break;
        case GL_INVALID_ENUM: err_str = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: err_str = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: err_str = "GL_INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW: err_str = "GL_STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW: err_str = "GL_STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY: err_str = "GL_OUT_OF_MEMORY"; break;
        default: err_str = "UNKNOWN_ERROR"; break;
        }
        FormattedError("GL Error", "GL error in file:\n%s\nAt line:\n%d\n%s", file, line, err_str);
        exit(1);
    }
}

int CreateShader(int type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)    {
        static const int kBufSize = 4096;
        GLchar err_msg[kBufSize];
        glGetShaderInfoLog(shader, kBufSize, NULL, err_msg);

        const char *shader_type_cstr = NULL;
        switch(type)
        {
        case GL_VERTEX_SHADER: shader_type_cstr = "vertex"; break;
        case GL_GEOMETRY_SHADER: shader_type_cstr = "geometry"; break;
        case GL_FRAGMENT_SHADER: shader_type_cstr = "fragment"; break;
        }

        FormattedError("Shader compile failed", "Error compiling %s shader:\n%s", shader_type_cstr, err_msg);
        return -1;
    }
    return shader;
}

 int CreateProgram(const int shaders[], int num_shaders) {
    int program = glCreateProgram();
    for(int i=0; i<num_shaders; ++i) {
        glAttachShader(program, shaders[i]);
    }
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)    {
        static const int kBufSize = 4096;
        GLchar err_msg[kBufSize];
        glGetProgramInfoLog(program, kBufSize, NULL, err_msg);
        FormattedError("Shader program link failed", "Error: %s", err_msg);
        return -1;
    }
    for(int i=0; i<num_shaders; ++i) {
        glDetachShader(program, shaders[i]);
    }
    return program;
}

 int CreateVBO(VBO_Type type, VBO_Hint hint, void* data, int num_data_elements) {
     int type_val;
     switch(type){
     case kArrayVBO: type_val = GL_ARRAY_BUFFER; break;
     case kElementVBO: type_val = GL_ELEMENT_ARRAY_BUFFER; break;
     default:
         FormattedError("Invalid VBO type", "CreateStaticVBO called with bad type");
         exit(1);
     }
     int hint_val;
     switch(hint){
     case kDynamicVBO: hint_val = GL_DYNAMIC_DRAW; break;
     case kStaticVBO: hint_val = GL_STATIC_DRAW; break;
     case kStreamVBO: hint_val = GL_STREAM_DRAW; break;
     default:
         FormattedError("Invalid VBO hint", "CreateStaticVBO called with bad hint");
         exit(1);
     }
     GLuint u_vbo;
     glGenBuffers(1, &u_vbo);
     glBindBuffer(type_val, u_vbo);
     if(num_data_elements > 0){
         glBufferData(type_val, num_data_elements, data, hint_val);
     }
     glBindBuffer(type_val, 0);
     return (int)u_vbo;
 }

 void InitGraphicsContext(GraphicsContext *graphics_context) {
    Profiler profiler;
    profiler.Init();
    graphics_context->screen_dims[0] = 1280;
    graphics_context->screen_dims[1] = 720;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, (kMSAA==0)?0:1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, kMSAA); // Why does this have to be before createwindow?
    graphics_context->window = SDL_CreateWindow("Under Glass", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        graphics_context->screen_dims[0], 
        graphics_context->screen_dims[1], 
        SDL_WINDOW_OPENGL);

    if(!graphics_context->window) {
        FormattedError("SDL_CreateWindow failed", "Could not create window: %s", SDL_GetError());
        exit(1);
    }

    graphics_context->gl_context = SDL_GL_CreateContext(graphics_context->window);
    if(!graphics_context->gl_context){
        FormattedError("SDL_GL_CreateContext failed", "Could not create GL context: %s", SDL_GetError());
        exit(1);
    }
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        FormattedError("glewInit failed", "Error: %s", glewGetErrorString(err));
        exit(1);
    }
    if (!GLEW_VERSION_3_2) {
        FormattedError("OpenGL 3.2 not supported", "OpenGL 3.2 is required");
        exit(1);
    }

    int multisample_buffers, multisample_samples;
    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &multisample_buffers);
    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &multisample_samples);
    SDL_Log("Multisample buffers: %d Multisample samples: %d\n", multisample_buffers, multisample_samples);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // Enable vSync
    if(!SDL_GL_SetSwapInterval(-1)) {
        SDL_GL_SetSwapInterval(1);
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
}

void InitGraphicsData(int *triangle_vbo, int *index_vbo) {
    static const float verts[] = {
        -100,  100, 0.0f, 0.0f,
         100,  100, 1.0f, 0.0f,
        -100, -100, 0.0f, 1.0f,
         100, -100, 1.0f, 1.0f,
    };

    static const unsigned indices[] = {
        0, 1, 2, 1, 3, 2
    };

    *triangle_vbo = CreateVBO(kArrayVBO, kStaticVBO, (void*)verts, sizeof(verts));
    *index_vbo = CreateVBO(kElementVBO, kStaticVBO, (void*)indices, sizeof(indices));

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    CHECK_GL_ERROR();
}

void BoxFilterHalve(unsigned char* data, int channels, int old_width, int old_height) {
    int new_width = old_width / 2;
    int new_height = old_height / 2;
    int temp_data_size = channels * new_width * new_height;
    unsigned char* temp_data = (unsigned char*)malloc(temp_data_size); // TODO: no temp alloc
    // TODO: This is very slow, make it faster once it works
    for(int new_x=0, old_x=0, len=old_width/2; 
        new_x<len; 
        ++new_x, old_x+=2)
    {
        for(int new_y=0, old_y=0, len=old_height/2;
            new_y<len; 
            ++new_y, old_y+=2)
        {
            int old_index = (old_x + old_y * old_width) * channels;
            int new_index = (new_x + new_y * new_width) * channels;
            for(int k=0; k<channels; ++k){
                temp_data[new_index+k] = (data[old_index+k] +
                                          data[old_index+k+channels] +
                                          data[old_index+k+channels + old_width*channels] +
                                          data[old_index+k + old_width*channels]) / 4;
            }
        }
    }
    memcpy(data, temp_data, temp_data_size);
    free(temp_data);
}

int GetPow2(int val, int* remainder) {
    SDL_assert(val>0);
    int ret = 0;
    int test = 1;
    while(test < val){
        ++ret;
        test *= 2;
    }
    if(test > val){
        test /= 2;
        --ret;
        *remainder = val - test;
    } else {
        *remainder = 0;
    }
    return ret;
}

static void TestPow2() {
    int test_ret, test_remainder;
    test_ret = GetPow2(256, &test_remainder);
    SDL_assert(test_ret == 8 && test_remainder == 0);
    test_ret = GetPow2(8, &test_remainder);
    SDL_assert(test_ret == 3 && test_remainder == 0);
    test_ret = GetPow2(12, &test_remainder);
    SDL_assert(test_ret == 3 && test_remainder == 4);
    test_ret = GetPow2(130, &test_remainder);
    SDL_assert(test_ret == 7 && test_remainder == 2);
}

int LoadImage(const char* path, FileLoadThreadData* file_load_data){
    int path_len = strlen(path);
    if(path_len > FileRequest::kMaxFileRequestPathLen){
        FormattedError("File path too long", "Path is %d characters, %d allowed", path_len, FileRequest::kMaxFileRequestPathLen);
        exit(1);
    }
    int texture = -1;
    if (SDL_LockMutex(file_load_data->mutex) == 0) {
        FileRequest* request = file_load_data->queue.AddNewRequest();
        for(int i=0; i<path_len + 1; ++i){
            request->path[i] = path[i];
        }
        request->condition = SDL_CreateCond();
        SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
        int x,y,comp;
        unsigned char *data = stbi_load_from_memory((const stbi_uc*)file_load_data->memory, file_load_data->memory_len, &x, &y, &comp, STBI_default);
        SDL_UnlockMutex(file_load_data->mutex);

        GLint internal_format = -1;
        switch(comp){
        case 1:
            internal_format = GL_LUMINANCE;
            break;
        case 3:
            internal_format = GL_RGB;
            break;
        case 4:
            internal_format = GL_RGBA;
            break;
        }
        GLuint tmp_texture;
        glGenTextures(1, &tmp_texture);
        texture = tmp_texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, x, y, 0, internal_format, GL_UNSIGNED_BYTE, data);
        CHECK_GL_ERROR();

        bool can_mip = true;
        int remainder;
        int x_mips = GetPow2(x, &remainder);
        if(remainder){
            can_mip = false; // Width is not power of 2
        }
        int y_mips = GetPow2(y, &remainder);
        if(remainder){
            can_mip = false; // Height is not power of 2
        }
        int num_mips = 0;
        if(can_mip){
            num_mips = min(x_mips, y_mips); // TODO: make this max() and separate box filter axes
            int dims[] = {x,y};
            for(int i=0; i<num_mips; ++i){
                BoxFilterHalve(data, comp, dims[0], dims[1]);
                dims[0] /= 2;
                dims[1] /= 2;
                glTexImage2D(GL_TEXTURE_2D, i+1, internal_format, dims[0], dims[1], 0, internal_format, GL_UNSIGNED_BYTE, data);
                CHECK_GL_ERROR();
            }
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max(0,num_mips-4)); // Don't quite allow mipmap down to 1 pixel
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, kMaxAnisotropy);
        stbi_image_free(data);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }
    return texture;
}

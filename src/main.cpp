#include "SDL.h"
#include "GL/glew.h"
#define GLM_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtx/quaternion.hpp"
#include <cstdio>
#include "fbx/fbx.h"
#include "platform_sdl/error.h"
#include "platform_sdl/audio.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/debug_draw.h"
#include "internal/common.h"
#include "internal/memory.h"
#include <cstring>
#include <sys/stat.h>
#include "platform_sdl/profiler.h"

#ifdef WIN32
#define ASSET_PATH "../assets/"
#else
#define ASSET_PATH "assets/"
#endif

using namespace glm;

enum VBO_Setup {
    kSimple_4V, // 4 vert
    kInterleave_3V2T3N, // 3 vert, 2 tex coord, 3 normal
    kInterleave_3V2T3N4I4W // 3 vert, 2 tex coord, 3 normal, 4 bone index, 4 bone weight
};

mat4 g_bone_transforms[128];
mat4 g_anim_bone_transforms[128];

struct Drawable {
    int texture_id;
    int vert_vbo;
    int index_vbo;
    int num_indices;
    int shader_id;
    VBO_Setup vbo_layout;
    mat4 transform;
};

struct SeparableTransform {
    quat rotation;
    vec3 scale;
    vec3 translation;
    mat4 GetCombination();
    SeparableTransform():scale(1.0f){}
};

struct GameState {
    SeparableTransform camera;
    float camera_fov;
    SeparableTransform character;
    int char_drawable;
    bool editor_mode;
};

mat4 SeparableTransform::GetCombination() {
    mat4 mat;
    for(int i=0; i<3; ++i){
        mat[i][i] = scale[i];
    }
    for(int i=0; i<3; ++i){
        mat[i] = rotation * mat[i];
    }
    for(int i=0; i<3; ++i){
        mat[3][i] += translation[i];
    }
    return mat;
}

struct DrawScene {
    static const int kMaxDrawables = 1000;
    Drawable drawables[kMaxDrawables];
    int num_drawables;
    DebugDrawLines lines;
};

void Update(GameState* game_state, const vec2& mouse_rel, float time_step) {
    float cam_speed = 10.0f;
    float char_speed = 4.0f;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_SPACE]) {
        cam_speed *= 0.1f;
    }
    static float cam_x = 0.0f;
    static float cam_y = 0.0f;
    if(game_state->editor_mode){
        if (state[SDL_SCANCODE_W]) {
            game_state->camera.translation -= game_state->camera.rotation * vec3(0,0,1) * cam_speed * time_step;
        }
        if (state[SDL_SCANCODE_S]) {
            game_state->camera.translation += game_state->camera.rotation * vec3(0,0,1) * cam_speed * time_step;
        }
        if (state[SDL_SCANCODE_A]) {
            game_state->camera.translation -= game_state->camera.rotation * vec3(1,0,0) * cam_speed * time_step;
        }
        if (state[SDL_SCANCODE_D]) {
            game_state->camera.translation += game_state->camera.rotation * vec3(1,0,0) * cam_speed * time_step;
        }
        const float kMouseSensitivity = 0.003f;
        Uint32 mouse_button_bitmask = SDL_GetMouseState(NULL, NULL);
        if(mouse_button_bitmask & SDL_BUTTON_LEFT){
            cam_x -= mouse_rel.y * kMouseSensitivity;
            cam_y -= mouse_rel.x * kMouseSensitivity;
        }
        quat xRot = angleAxis(cam_x,vec3(1,0,0));
        quat yRot = angleAxis(cam_y,vec3(0,1,0));
        game_state->camera.rotation = yRot * xRot;
        game_state->camera_fov = 1.02f;
    } else {
        cam_x = -0.75f;
        cam_y = 1.0f;
        quat xRot = angleAxis(cam_x,vec3(1,0,0));
        quat yRot = angleAxis(cam_y,vec3(0,1,0));
        game_state->camera.rotation = yRot * xRot;

        vec3 temp = game_state->camera.rotation * vec3(0,0,-1);
        vec3 cam_north = normalize(vec3(temp[0], 0.0f, temp[1]));
        vec3 cam_east = normalize(vec3(-cam_north[2], 0.0f, cam_north[0]));

        vec3 target_dir;
        float target_speed = 0.0f;
        if (state[SDL_SCANCODE_W]) {
            target_dir += cam_north;
            target_speed = 1.0f;
        }
        if (state[SDL_SCANCODE_S]) {
            target_dir -= cam_north;
            target_speed = 1.0f;
        }
        if (state[SDL_SCANCODE_D]) {
            target_dir += cam_east;
            target_speed = 1.0f;
        }
        if (state[SDL_SCANCODE_A]) {
            target_dir -= cam_east;
            target_speed = 1.0f;
        }

        static float char_rotation = 0.0f;
        static const float turn_speed = 10.0f;
        if(target_speed > 0.0f && length(target_dir) > 0.0f){
            game_state->character.translation += normalize(target_dir) * 
                target_speed * char_speed * time_step;

            float target_rotation = -atan2f(target_dir[2], target_dir[0])+half_pi<float>();
            /*float rel_rotation = target_rotation - char_rotation;
            float temp;
            rel_rotation = modf<float>(rel_rotation / pi<float>(), temp) * pi<float>();
            if(fabsf(rel_rotation) < turn_speed * time_step){
                char_rotation += rel_rotation;
            } else {
                char_rotation += rel_rotation>0.0f?1.0f:-1.0f * turn_speed * time_step;
            }*/
            game_state->character.rotation = angleAxis(target_rotation, vec3(0,1,0)); 
        }

        game_state->camera.translation = game_state->character.translation +
            game_state->camera.rotation * vec3(0,0,1) * 10.0f;
        game_state->camera_fov = 0.8f;
    }
    static bool old_tab = false;
    if (state[SDL_SCANCODE_TAB] && !old_tab) {
        game_state->editor_mode = !game_state->editor_mode;
    }
    old_tab = (state[SDL_SCANCODE_TAB] != 0);

    game_state->camera.scale = vec3(1.0f);
}

void DrawCoordinateGrid(DrawScene* draw_scene){
    static const float opac = 0.25f;
    static const vec4 basic_grid_color(1.0f, 1.0f, 1.0f, opac);
    static const vec4 x_axis_color(1.0f, 0.0f, 0.0f, opac);
    static const vec4 y_axis_color(0.0f, 1.0f, 0.0f, opac);
    static const vec4 z_axis_color(0.0f, 0.0f, 1.0f, opac);
    for(int i=-10; i<11; ++i){
        draw_scene->lines.Add(vec3(-10.0f, 0.0f, i), vec3(10.0f, 0.0f, i),
                              i==0?x_axis_color:basic_grid_color, kDraw, 1);
        draw_scene->lines.Add(vec3(i, 0.0f, -10.0f), vec3(i, 0.0f, 10.0f),
                              i==0?z_axis_color:basic_grid_color, kDraw, 1);
    }
    draw_scene->lines.Add(vec3(0.0f, -10.0f, 0.0f), vec3(0.0f, 10.0f,  0.0f),
                          y_axis_color, kDraw, 1);
}

void DrawDrawable(const mat4 &proj_view_mat, Drawable* drawable) {
    glUseProgram(drawable->shader_id);

    GLuint modelview_matrix_uniform = glGetUniformLocation(drawable->shader_id, "mv_mat");
    GLuint normal_matrix_uniform = glGetUniformLocation(drawable->shader_id, "norm_mat");

    mat4 model_mat = drawable->transform;
    mat4 mat = proj_view_mat * model_mat;
    mat3 normal_mat = mat3(model_mat);
    glUniformMatrix3fv(normal_matrix_uniform, 1, false, (GLfloat*)&normal_mat);

    GLuint texture_uniform = glGetUniformLocation(drawable->shader_id, "texture_id");
    glUniform1i(texture_uniform, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawable->texture_id);

    glBindBuffer(GL_ARRAY_BUFFER, drawable->vert_vbo);
    switch(drawable->vbo_layout){
    case kSimple_4V:
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&mat);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
        glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(0);
        break;
    case kInterleave_3V2T3N:
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&mat);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void*)(5*sizeof(GLfloat)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
        glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        break;
    case kInterleave_3V2T3N4I4W: {
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&proj_view_mat);
        GLuint bone_transforms_uniform = glGetUniformLocation(drawable->shader_id, "bone_matrices");
        for(int i=0; i<128; ++i){
            g_bone_transforms[i] = model_mat * g_anim_bone_transforms[i];
        }
        glUniformMatrix4fv(bone_transforms_uniform, 128, false, (GLfloat*)&g_bone_transforms);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16*sizeof(GLfloat), 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16*sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 16*sizeof(GLfloat), (void*)(5*sizeof(GLfloat)));
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 16*sizeof(GLfloat), (void*)(8*sizeof(GLfloat)));
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 16*sizeof(GLfloat), (void*)(12*sizeof(GLfloat)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
        glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(4);
        glDisableVertexAttribArray(3);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        } break;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    CHECK_GL_ERROR();
}

void Draw(GraphicsContext* context, GameState* game_state, DrawScene* draw_scene, int ticks) {
    glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
    glClearColor(0.5,0.5,0.5,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat = glm::perspective(game_state->camera_fov, aspect_ratio, 0.1f, 100.0f);
    mat4 view_mat = inverse(game_state->camera.GetCombination());
    mat4 proj_view_mat = proj_mat * view_mat;

    draw_scene->drawables[game_state->char_drawable].transform = 
        game_state->character.GetCombination();

    for(int i=0; i<draw_scene->num_drawables; ++i){
        Drawable* drawable = &draw_scene->drawables[i];
        DrawDrawable(proj_view_mat, drawable);
    }

    static const bool draw_coordinate_grid = false;
    if(draw_coordinate_grid){
        DrawCoordinateGrid(draw_scene);
    }
    draw_scene->lines.Draw(proj_view_mat);
}

void LoadFBX(FBXParseScene* parse_scene, const char* path, FileLoadThreadData* file_load_data, const char* specific_name) {
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
        const char** names = &specific_name;
        ParseFBXFromRAM(parse_scene, file_load_data->memory, file_load_data->memory_len, names, specific_name?1:0);
        SDL_UnlockMutex(file_load_data->mutex);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }
}

int CreateProgramFromFile(FileLoadThreadData* file_load_data, const char* path){
    int path_len = strlen(path)+5;
    if(path_len > FileRequest::kMaxFileRequestPathLen){
        FormattedError("File path too long", "Path is %d characters, %d allowed", path_len, FileRequest::kMaxFileRequestPathLen);
        exit(1);
    }

    char shader_path[FileRequest::kMaxFileRequestPathLen];
    FormatString(shader_path, FileRequest::kMaxFileRequestPathLen, "%s.vert", path);
    static const int kNumShaders = 2;
    int shaders[kNumShaders];

    if (SDL_LockMutex(file_load_data->mutex) == 0) {
        FileRequest* request = file_load_data->queue.AddNewRequest();
        for(int i=0; i<path_len + 1; ++i){
            request->path[i] = shader_path[i];
        }
        request->condition = SDL_CreateCond();
        SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
        char* mem_text = (char*)file_load_data->memory;
        mem_text[file_load_data->memory_len] = '\0';
        shaders[0] = CreateShader(GL_VERTEX_SHADER, mem_text);
        SDL_UnlockMutex(file_load_data->mutex);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }

    FormatString(shader_path, FileRequest::kMaxFileRequestPathLen, "%s.frag", path);

    if (SDL_LockMutex(file_load_data->mutex) == 0) {
        FileRequest* request = file_load_data->queue.AddNewRequest();
        for(int i=0; i<path_len + 1; ++i){
            request->path[i] = shader_path[i];
        }
        request->condition = SDL_CreateCond();
        SDL_CondWait(request->condition, file_load_data->mutex);
        if(file_load_data->err){
            FormattedError(file_load_data->err_title, file_load_data->err_msg);
            exit(1);
        }
        char* mem_text = (char*)file_load_data->memory;
        mem_text[file_load_data->memory_len] = '\0';
        shaders[1] = CreateShader(GL_FRAGMENT_SHADER, mem_text);
        SDL_UnlockMutex(file_load_data->mutex);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }

    int shader_program = CreateProgram(shaders, kNumShaders);
    for(int i=0; i<kNumShaders; ++i){
        glDeleteShader(shaders[i]);
    }
    return shader_program;
}

void VBOFromMesh(Mesh* mesh, int* vert_vbo, int* index_vbo) {
    int interleaved_size = sizeof(float)*mesh->num_tris*3*8;
    float* interleaved = (float*)malloc(interleaved_size);
    int consecutive_size = sizeof(unsigned)*mesh->num_tris*3;
    unsigned* consecutive = (unsigned*)malloc(consecutive_size);
    for(int i=0, index=0, len=mesh->num_tris*3; i<len; ++i){
        for(int j=0; j<3; ++j){
            interleaved[index++] = mesh->vert_coords[mesh->tri_indices[i]*3+j];
        }
        for(int j=0; j<2; ++j){
            interleaved[index++] = mesh->tri_uvs[i*2+j];
        }
        for(int j=0; j<3; ++j){
            interleaved[index++] = mesh->tri_normals[i*3+j];
        }
        consecutive[i] = i;
    }    
    *vert_vbo = CreateVBO(kArrayVBO, kStaticVBO, interleaved, interleaved_size);
    *index_vbo = CreateVBO(kElementVBO, kStaticVBO, consecutive, consecutive_size);
}

void VBOFromSkinnedMesh(Mesh* mesh, int* vert_vbo, int* index_vbo) {
    int interleaved_size = sizeof(float)*mesh->num_tris*3*(3+2+3+4+4);
    float* interleaved = (float*)malloc(interleaved_size);
    int consecutive_size = sizeof(unsigned)*mesh->num_tris*3;
    unsigned* consecutive = (unsigned*)malloc(consecutive_size);
    for(int i=0, index=0, len=mesh->num_tris*3; i<len; ++i){
        int vert = mesh->tri_indices[i];
        for(int j=0; j<3; ++j){
            interleaved[index++] = mesh->vert_coords[vert*3+j];
        }
        for(int j=0; j<2; ++j){
            interleaved[index++] = mesh->tri_uvs[i*2+j];
        }
        for(int j=0; j<3; ++j){
            interleaved[index++] = mesh->tri_normals[i*3+j];
        }
        for(int j=0; j<4; ++j){
            interleaved[index++] = (float)mesh->vert_bone_indices[vert*4+j];
        }
        for(int j=0; j<4; ++j){
            interleaved[index++] = mesh->vert_bone_weights[vert*4+j];
        }
        consecutive[i] = i;
    }    
    *vert_vbo = CreateVBO(kArrayVBO, kStaticVBO, interleaved, interleaved_size);
    *index_vbo = CreateVBO(kElementVBO, kStaticVBO, consecutive, consecutive_size);
}

void GetBoundingBox(const Mesh* mesh, vec3* bounding_box) {
    SDL_assert(mesh);
    SDL_assert(mesh->num_verts > 0);
    bounding_box[0] = vec3(FLT_MAX);
    bounding_box[1] = vec3(-FLT_MAX);
    for(int i=0, vert_index=0; i<mesh->num_verts; ++i){
        for(int k=0; k<3; ++k){
            bounding_box[0][k] = min(mesh->vert_coords[vert_index], bounding_box[0][k]);
            bounding_box[1][k] = max(mesh->vert_coords[vert_index], bounding_box[1][k]);
            ++vert_index;
        }
    }
}

static const int s_pos_x = 1 << 0, s_pos_y = 1 << 1, s_pos_z = 1 << 2;
static const int e_pos_x = 1 << 3, e_pos_y = 1 << 4, e_pos_z = 1 << 5;

static void AddBBLine(DebugDrawLines* lines, const mat4& mat, vec3 bb[], int flags) {
    vec3 points[2];
    points[0][0] = (flags & s_pos_x)?bb[1][0]:bb[0][0];
    points[0][1] = (flags & s_pos_y)?bb[1][1]:bb[0][1];
    points[0][2] = (flags & s_pos_z)?bb[1][2]:bb[0][2];
    points[1][0] = (flags & e_pos_x)?bb[1][0]:bb[0][0];
    points[1][1] = (flags & e_pos_y)?bb[1][1]:bb[0][1];
    points[1][2] = (flags & e_pos_z)?bb[1][2]:bb[0][2];
    lines->Add(vec3(mat*vec4(points[0],1.0f)), 
               vec3(mat*vec4(points[1],1.0f)), 
               vec4(1.0f), kPersistent, 1);
}

static void DrawBoundingBox(DebugDrawLines* lines, const mat4& mat, vec3 bb[]) {
    static const int s_neg_x = 0, s_neg_y = 0, s_neg_z = 0;
    static const int e_neg_x = 0, e_neg_y = 0, e_neg_z = 0;
    // Neg Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_neg_z);
    // Pos Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_neg_z);
    // Neg Y to Pos Y
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_neg_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
}

struct BoneIDSort {
    uint64_t unique_id;
    int num;
};

int BoneIDSortCompare(const void* a_ptr, const void* b_ptr){
    const BoneIDSort* a = (const BoneIDSort*)a_ptr;
    const BoneIDSort* b = (const BoneIDSort*)b_ptr;
    if(a->unique_id > b->unique_id) {
        return 1;
    } else if(a->unique_id == b->unique_id){
        return 0;
    } else {
        return -1;
    }
} 

void AttachMeshToSkeleton(Mesh* mesh, Skeleton* skeleton) {
    // rearrange mesh bone indices so they correspond to skeleton bones
    BoneIDSort* mesh_bone_ids = (BoneIDSort*)malloc(sizeof(BoneIDSort) * mesh->num_bones);
    for(int i=0; i<mesh->num_bones; ++i){
        mesh_bone_ids[i].num = i;
        mesh_bone_ids[i].unique_id = mesh->bone_ids[i];
    }
    qsort(mesh_bone_ids, mesh->num_bones, sizeof(BoneIDSort), BoneIDSortCompare);
    BoneIDSort* skeleton_bone_ids = (BoneIDSort*)malloc(sizeof(BoneIDSort) * skeleton->num_bones);
    for(int i=0; i<skeleton->num_bones; ++i){
        skeleton_bone_ids[i].num = i;
        skeleton_bone_ids[i].unique_id = skeleton->bones[i].bone_id;
    }
    qsort(skeleton_bone_ids, skeleton->num_bones, sizeof(BoneIDSort), BoneIDSortCompare);
    int* new_bone_ids = (int*)malloc(sizeof(int) * mesh->num_bones);
    for(int i=0; i<mesh->num_bones; ++i){
        new_bone_ids[i] = -1;
    }
    for(int mesh_index=0, skeleton_index=0; 
        mesh_index < mesh->num_bones && skeleton_index < skeleton->num_bones;)
    {
        BoneIDSort& skel = skeleton_bone_ids[skeleton_index];
        BoneIDSort& mesh = mesh_bone_ids[mesh_index];
        if(skel.unique_id == mesh.unique_id){
            new_bone_ids[mesh.num] = skel.num;
            ++mesh_index;
            ++skeleton_index;
        } else if(skel.unique_id > mesh.unique_id) {
            ++mesh_index;
        } else {
            ++skeleton_index;
        }
    }
    for(int i=0, index=0; i<mesh->num_verts*4; ++i){
        int& vert_bone_index = mesh->vert_bone_indices[index++];
        if(vert_bone_index != -1){
            vert_bone_index = new_bone_ids[vert_bone_index];
        }
    }

    // What to do with mesh->bind_matrices
    float* bind_matrices = (float*)malloc(16 * sizeof(float) * skeleton->num_bones);
    for(int i=0, index=0; i<skeleton->num_bones; ++i){
        for(int j=0; j<16; ++j){
            bind_matrices[index++] = (j/4 == j%4)?1.0f:0.0f;
        }
    }
    for(int i=0; i<mesh->num_bones; ++i){
        if(new_bone_ids[i] != -1){
            void *dst = &bind_matrices[new_bone_ids[i]*16];
            void *src = &mesh->bind_matrices[i*16];
            memcpy(dst, src, sizeof(float) * 16);
        }
    }
    free(mesh->bind_matrices);
    mesh->bind_matrices = bind_matrices;

    free(new_bone_ids);
    free(skeleton_bone_ids);
    free(mesh_bone_ids);
}

int main(int argc, char* argv[]) {
    Profiler profiler;
    profiler.Init();

    profiler.StartEvent("Allocate game memory block");
    static const int kGameMemSize = 1024*1024*64;
    StackMemoryBlock stack_memory_block;
    stack_memory_block.Init(malloc(kGameMemSize), kGameMemSize);
    if(!stack_memory_block.mem){
        FormattedError("Malloc failed", "Could not allocate enough memory");
        exit(1);
    }
    profiler.EndEvent();

    profiler.StartEvent("Initializing SDL");
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
        FormattedError("SDL_Init failed", "Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }
    profiler.EndEvent();

    char* write_dir = SDL_GetPrefPath("Wolfire", "UnderGlass");

    profiler.StartEvent("Checking for assets folder");
    {
        struct stat st;
        if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
            char *basePath = SDL_GetBasePath();
            ChangeWorkingDirectory(basePath);
            SDL_free(basePath);
            if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
                FormattedError("Assets?", "Could not find assets directory, possibly running from inside archive");
                exit(1);
            }
        }
    }
    profiler.EndEvent();

    profiler.StartEvent("Set up file loader");
    FileLoadThreadData file_load_thread_data;
    file_load_thread_data.memory_len = 0;
    file_load_thread_data.memory = stack_memory_block.Alloc(FileLoadThreadData::kMaxFileLoadSize);
    if(!file_load_thread_data.memory){
        FormattedError("Alloc failed", "Could not allocate memory for FileLoadData");
        return 1;
    }
    file_load_thread_data.wants_to_quit = false;
    file_load_thread_data.mutex = SDL_CreateMutex();
    if (!file_load_thread_data.mutex) {
        FormattedError("SDL_CreateMutex failed", "Could not create file load mutex: %s", SDL_GetError());
        return 1;
    }
    SDL_Thread* file_thread = SDL_CreateThread(FileLoadAsync, "FileLoaderThread", &file_load_thread_data);
    if(!file_thread){
        FormattedError("SDL_CreateThread failed", "Could not create file loader thread: %s", SDL_GetError());
        return 1;
    }
    profiler.EndEvent();

    profiler.StartEvent("Set up graphics context");
    GraphicsContext graphics_context;
    InitGraphicsContext(&graphics_context);
    profiler.EndEvent();

    AudioContext audio_context;
    InitAudio(&audio_context, &stack_memory_block);

    // Game code starts here
    profiler.StartEvent("Parsing lamp fbx and creating vbo");
    FBXParseScene parse_scene;
    int street_lamp_vert_vbo, street_lamp_index_vbo;
    LoadFBX(&parse_scene, ASSET_PATH "street_lamp.fbx", &file_load_thread_data, NULL);
    int num_street_lamp_indices = parse_scene.meshes[0].num_tris*3;
    vec3 lamp_bb[2];
    GetBoundingBox(&parse_scene.meshes[0], lamp_bb);
    VBOFromMesh(&parse_scene.meshes[0], &street_lamp_vert_vbo, &street_lamp_index_vbo);
    parse_scene.Dispose();
    profiler.EndEvent();

    profiler.StartEvent("Parsing cobble fbx and creating vbo");
    int cobble_floor_vert_vbo, cobble_floor_index_vbo;
    LoadFBX(&parse_scene, ASSET_PATH "cobble_floor.fbx", &file_load_thread_data, NULL);    
    int num_cobble_floor_indices = parse_scene.meshes[0].num_tris*3;
    vec3 floor_bb[2];
    GetBoundingBox(&parse_scene.meshes[0], floor_bb);
    VBOFromMesh(&parse_scene.meshes[0], &cobble_floor_vert_vbo, &cobble_floor_index_vbo);
    parse_scene.Dispose();
    profiler.EndEvent();

    profiler.StartEvent("Loading textures");
    int cobbles_texture = LoadImage(ASSET_PATH "cobbles_c.tga", &file_load_thread_data);
    int lamp_texture = LoadImage(ASSET_PATH "lamp_c.tga", &file_load_thread_data);
    int character_texture = LoadImage(ASSET_PATH "main_character_c.tga", &file_load_thread_data);
    profiler.EndEvent();

    profiler.StartEvent("Loading shader program");
    int shader_3d_model = CreateProgramFromFile(&file_load_thread_data, ASSET_PATH "shaders/3D_model");
    int shader_3d_model_skinned = CreateProgramFromFile(&file_load_thread_data, ASSET_PATH "shaders/3D_model_skinned");
    profiler.EndEvent();

    GameState game_state;
    game_state.camera.translation = vec3(0.0f,0.0f,20.0f);
    game_state.camera.rotation = angleAxis(0.0f,vec3(1.0f,0.0f,0.0f));
    game_state.camera.scale = vec3(1.0f);
    game_state.editor_mode = false;

    DrawScene draw_scene;
    draw_scene.lines.num_lines = 0;
    draw_scene.num_drawables = 0;
    draw_scene.drawables[0].vert_vbo = street_lamp_vert_vbo;
    draw_scene.drawables[0].index_vbo = street_lamp_index_vbo;
    draw_scene.drawables[0].num_indices = num_street_lamp_indices;
    draw_scene.drawables[0].vbo_layout = kInterleave_3V2T3N;
    SeparableTransform sep_transform;
    sep_transform.rotation = angleAxis(-glm::half_pi<float>(), vec3(1.0f,0.0f,0.0f));
    sep_transform.translation = vec3(0.0f, -lamp_bb[0][2], 2.0f);
    sep_transform.scale = vec3(1.0f);
    draw_scene.drawables[0].transform = sep_transform.GetCombination();
    draw_scene.drawables[0].texture_id = lamp_texture;
    draw_scene.drawables[0].shader_id = shader_3d_model;
    ++draw_scene.num_drawables;

    draw_scene.lines.shader = CreateProgramFromFile(&file_load_thread_data, ASSET_PATH "shaders/debug_draw");
    draw_scene.lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);

    int character_vert_vbo, character_index_vbo, num_character_indices;
    {
        profiler.StartEvent("Parsing character fbx");
        profiler.StartEvent("Loading file to RAM");
        FBXParseScene parse_scene;
        void* mem = malloc(1024*1024*64);
        int len;
        char err_title[FileLoadThreadData::kMaxErrMsgLen];
        char err_msg[FileLoadThreadData::kMaxErrMsgLen];
        if(!FileLoadThreadData::LoadFile(ASSET_PATH "main_character_rig.fbx", 
            mem,  &len, err_title, err_msg))
        {
            FormattedError(err_title, err_msg);
            exit(1);
        }
        profiler.EndEvent();

        profiler.StartEvent("Parsing character fbx");
        const char* names[] = {"RiggedMesh", "rig"};
        ParseFBXFromRAM(&parse_scene, mem, len, names, 2);
        Mesh& mesh = parse_scene.meshes[0];
        Skeleton& skeleton = parse_scene.skeletons[0];
        profiler.EndEvent();

        AttachMeshToSkeleton(&mesh, &skeleton);
        for(int bone_index=0; bone_index<skeleton.num_bones; ++bone_index){
            Bone& bone = skeleton.bones[bone_index];
            mat4 temp;
            for(int matrix_element=0; matrix_element<16; ++matrix_element){
                temp[matrix_element/4][matrix_element%4] = bone.transform[matrix_element];
            }
            mat4 bind_mat;
            for(int j=0; j<16; ++j){
                bind_mat[j/4][j%4] = mesh.bind_matrices[bone_index*16+j];
            }
            mat4 rot_mat = toMat4(angleAxis(-glm::half_pi<float>(), vec3(1.0f,0.0f,0.0f)));
            g_anim_bone_transforms[bone_index] = temp * inverse(bind_mat) * rot_mat;
        }
        
        profiler.StartEvent("Creating VBO and adding to scene");
        vec3 char_bb[2];
        GetBoundingBox(&mesh, char_bb);
        VBOFromSkinnedMesh(&mesh, &character_vert_vbo, &character_index_vbo);
        num_character_indices = mesh.num_tris*3;

        game_state.char_drawable = draw_scene.num_drawables;
        draw_scene.drawables[draw_scene.num_drawables].vert_vbo = character_vert_vbo;
        draw_scene.drawables[draw_scene.num_drawables].index_vbo = character_index_vbo;
        draw_scene.drawables[draw_scene.num_drawables].num_indices = num_character_indices;
        draw_scene.drawables[draw_scene.num_drawables].vbo_layout = kInterleave_3V2T3N4I4W;
        SeparableTransform char_transform;
        char_transform.scale = vec3(1.0f);
        draw_scene.drawables[draw_scene.num_drawables].transform = mat4();//char_transform.GetCombination();
        draw_scene.drawables[draw_scene.num_drawables].texture_id = character_texture;
        draw_scene.drawables[draw_scene.num_drawables].shader_id = shader_3d_model_skinned;
        ++draw_scene.num_drawables;
        profiler.EndEvent();

        draw_scene.lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);
        parse_scene.Dispose();
        profiler.EndEvent();
    }

    for(int i=-10; i<10; ++i){
        for(int j=-10; j<10; ++j){
            sep_transform.translation = vec3(0.0f, floor_bb[1][2], 0.0f);
            sep_transform.translation += vec3(-1.0f+(float)j*2.0f,0.0f,-1.0f+(float)i*2.0f);
            draw_scene.drawables[draw_scene.num_drawables].vert_vbo = cobble_floor_vert_vbo;
            draw_scene.drawables[draw_scene.num_drawables].index_vbo = cobble_floor_index_vbo;
            draw_scene.drawables[draw_scene.num_drawables].num_indices = num_cobble_floor_indices;
            draw_scene.drawables[draw_scene.num_drawables].vbo_layout = kInterleave_3V2T3N;
            draw_scene.drawables[draw_scene.num_drawables].transform = sep_transform.GetCombination();
            draw_scene.drawables[draw_scene.num_drawables].texture_id = cobbles_texture;
            draw_scene.drawables[draw_scene.num_drawables].shader_id = shader_3d_model;
            ++draw_scene.num_drawables;
        }
    }

    int last_ticks = SDL_GetTicks();
    bool game_running = true;
    while(game_running){
        profiler.StartEvent("Game loop");
        SDL_Event event;
        vec2 mouse_rel;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_QUIT:
                game_running = false;
                break;
            case SDL_MOUSEMOTION:
                mouse_rel[0] += event.motion.xrel;
                mouse_rel[1] += event.motion.yrel;
                break;
            }
        }
        profiler.StartEvent("Draw");
        Draw(&graphics_context, &game_state, &draw_scene, SDL_GetTicks());
        profiler.EndEvent();
        profiler.StartEvent("Update");
        int ticks = SDL_GetTicks();
        Update(&game_state, mouse_rel, (ticks - last_ticks) / 1000.0f);
        last_ticks = ticks;
        profiler.EndEvent();
        profiler.StartEvent("Audio");
        UpdateAudio(&audio_context);
        profiler.EndEvent();
        profiler.StartEvent("Swap");
        SDL_GL_SwapWindow(graphics_context.window);
        profiler.EndEvent();
        profiler.EndEvent();
    }
    
    // Game code ends here

    {
        static const int kMaxPathSize = 4096;
        char path[kMaxPathSize];
        FormatString(path, kMaxPathSize, "%sprofile_data.txt", write_dir);
        profiler.Export(path);
    }

    // Wait for the audio to fade out
    // TODO: handle this better -- e.g. force audio fade immediately
    SDL_Delay(200);

    // We can probably just skip most of this if we want to quit faster
    SDL_CloseAudioDevice(audio_context.device_id);
    SDL_GL_DeleteContext(graphics_context.gl_context);  
    SDL_DestroyWindow(graphics_context.window);
    // Cleanly shut down file load thread 
    if (SDL_LockMutex(file_load_thread_data.mutex) == 0) {
        file_load_thread_data.wants_to_quit = true;
        SDL_UnlockMutex(file_load_thread_data.mutex);
        SDL_WaitThread(file_thread, NULL);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }
    SDL_free(write_dir);
    SDL_Quit();
    free(stack_memory_block.mem);
    return 0;
}
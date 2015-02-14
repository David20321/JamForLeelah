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
#include "platform_sdl/audio.h"
#include "platform_sdl/debug_draw.h"
#include "platform_sdl/debug_text.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/profiler.h"
#include "internal/common.h"
#include "internal/memory.h"
#include "internal/separable_transform.h"
#include <cstring>
#include <sys/stat.h>
#include "stb_truetype.h"

#ifdef WIN32
#define ASSET_PATH "../assets/"
#else
#define ASSET_PATH "assets/"
#endif

const char* asset_list[] = {
    ASSET_PATH "art/street_lamp.fbx",
    ASSET_PATH "art/lamp_c.tga",
    ASSET_PATH "art/dry_fountain.fbx",
    ASSET_PATH "art/dry_fountain_c.tga",
    ASSET_PATH "art/flower_box.fbx",
    ASSET_PATH "art/flowerbox_c.tga",
    ASSET_PATH "art/garden_tall_corner.fbx",
    ASSET_PATH "art/garden_tall_corner_c.tga",
    ASSET_PATH "art/garden_tall_nook.fbx",
    ASSET_PATH "art/garden_tall_nook_c.tga",
    ASSET_PATH "art/garden_tall_stairs.fbx",
    ASSET_PATH "art/garden_tall_stairs.tga",
    ASSET_PATH "art/garden_tall_wall.fbx",
    ASSET_PATH "art/garden_tall_wall_c.tga",
    ASSET_PATH "art/short_wall.fbx",
    ASSET_PATH "art/short_wall_c.tga",
    ASSET_PATH "art/tree.fbx",
    ASSET_PATH "art/tree_c.tga",
    ASSET_PATH "art/wall_pillar.fbx",
    ASSET_PATH "art/wall_pillar_c.tga",
    ASSET_PATH "art/floor_quad.fbx",
    ASSET_PATH "art/tiling_cobbles_c.tga",
    ASSET_PATH "art/main_character_rig.fbx",
    ASSET_PATH "art/main_character_c.tga",
    ASSET_PATH "fonts/LiberationMono-Regular.ttf",
    ASSET_PATH "shaders/3D_model",
    ASSET_PATH "shaders/3D_model_skinned",
    ASSET_PATH "shaders/debug_draw",
    ASSET_PATH "shaders/debug_draw_text"
};

enum {
    kFBXLamp,
    kTexLamp,
    kFBXFountain,
    kTexFountain,
    kFBXFlowerbox,
    kTexFlowerbox,
    kFBXGardenTallCorner,
    kTexGardenTallCorner,
    kFBXGardenTallNook,
    kTexGardenTallNook,
    kFBXGardenTallStairs,
    kTexGardenTallStairs,
    kFBXGardenTallWall,
    kTexGardenTallWall,
    kFBXShortWall,
    kTexShortWall,
    kFBXTree,
    kTexTree,
    kFBXWallPillar,
    kTexWallPillar,
    kFBXFloor,
    kTexFloor,
    kFBXChar,
    kTexChar,
    kFontDebug,
    kShader3DModel,
    kShader3DModelSkinned,
    kShaderDebugDraw,
    kShaderDebugDrawText
};

using namespace glm;

struct Character {
    vec3 velocity;
    SeparableTransform transform;
    mat4 display_bone_transforms[128];
    mat4 local_bone_transforms[128];
};

struct Camera {
    float rotation_x;
    float rotation_y;
    vec3 position;
    quat GetRotation();
    mat4 GetMatrix();
};

quat Camera::GetRotation() {
    quat xRot = angleAxis(rotation_x, vec3(1,0,0));
    quat yRot = angleAxis(rotation_y, vec3(0,1,0));
    return yRot * xRot;
}

glm::mat4 Camera::GetMatrix() {
    SeparableTransform temp;
    temp.translation = position;
    temp.rotation = GetRotation();
    return temp.GetCombination();
}

struct GameState {
    static const int kMaxDrawables = 1000;
    Drawable drawables[kMaxDrawables];
    int num_drawables;
    DebugDrawLines lines;
    DebugText debug_text;
    float camera_fov;
    Character character;
    Camera camera;
    int char_drawable;
    bool editor_mode;
    TextAtlas text_atlas;

    void Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data);
};

void StartLoadFile(const char* path, FileLoadThreadData* file_load_data) {
    int path_len = strlen(path);
    if(path_len > FileRequest::kMaxFileRequestPathLen){
        FormattedError("File path too long", "Path is %d characters, %d allowed", path_len, FileRequest::kMaxFileRequestPathLen);
        exit(1);
    }
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
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }
}

void EndLoadFile(FileLoadThreadData* file_load_data) {
    SDL_UnlockMutex(file_load_data->mutex);
}


void LoadFBX(FBXParseScene* parse_scene, const char* path, FileLoadThreadData* file_load_data, const char* specific_name) {
    StartLoadFile(path, file_load_data);
    const char** names = &specific_name;
    ParseFBXFromRAM(parse_scene, file_load_data->memory, file_load_data->memory_len, names, specific_name?1:0);
    EndLoadFile(file_load_data);
}

void LoadTTF(const char* path, TextAtlas* text_atlas, FileLoadThreadData* file_load_data, float pixel_height) {
    StartLoadFile(path, file_load_data);
    static const int kAtlasSize = 512;
    unsigned char temp_bitmap[kAtlasSize*kAtlasSize];
    stbtt_BakeFontBitmap((const unsigned char*)file_load_data->memory, 0, 
        pixel_height, temp_bitmap, 512, 512, 32, 96, text_atlas->cdata); // no guarantee this fits!
    SDL_UnlockMutex(file_load_data->mutex);
    GLuint tmp_texture;
    glGenTextures(1, &tmp_texture);
    text_atlas->texture = tmp_texture;
    text_atlas->pixel_height = pixel_height;
    glBindTexture(GL_TEXTURE_2D, text_atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kAtlasSize, kAtlasSize, 0,
        GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    EndLoadFile(file_load_data);
}

int CreateProgramFromFile(FileLoadThreadData* file_load_data, const char* path){
    char shader_path[FileRequest::kMaxFileRequestPathLen];
    static const int kNumShaders = 2;
    int shaders[kNumShaders];

    for(int i=0; i<kNumShaders; ++i){
        FormatString(shader_path, FileRequest::kMaxFileRequestPathLen, 
            (i==0)?"%s.vert":"%s.frag", path);
        StartLoadFile(shader_path, file_load_data);
        char* mem_text = (char*)file_load_data->memory;
        mem_text[file_load_data->memory_len] = '\0';
        shaders[i] = CreateShader(i==0?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER, mem_text);
        EndLoadFile(file_load_data);
    }
    int shader_program = CreateProgram(shaders, kNumShaders);
    for(int i=0; i<kNumShaders; ++i){
        glDeleteShader(shaders[i]);
    }
    return shader_program;
}

void VBOFromMesh(const Mesh* mesh, int* vert_vbo, int* index_vbo) {
    // TODO: remove duplicated verts
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

void RecalculateNormals(Mesh* mesh){
    for(int tri_index=0; tri_index<mesh->num_tris; ++tri_index){
        vec3 tri_verts[3];
        for(int tri_vert=0; tri_vert<3; ++tri_vert){
            int vert = mesh->tri_indices[tri_index*3+tri_vert];
            for(int vert_comp=0; vert_comp<3; ++vert_comp){
                tri_verts[tri_vert][vert_comp] = mesh->vert_coords[vert*3+vert_comp];
            }            
        }
        vec3 normal = normalize(cross(tri_verts[1] - tri_verts[0], 
                                      tri_verts[2] - tri_verts[0]));
        for(int tri_vert=0; tri_vert<3; ++tri_vert){
            int vert = tri_index*3+tri_vert;
            for(int vert_comp=0; vert_comp<3; ++vert_comp){
                mesh->tri_normals[vert*3+vert_comp] = normal[vert_comp];
            }            
        }
    }    
}

struct MeshAsset {
    int vert_vbo;
    int index_vbo;
    int num_index;
    vec3 bounding_box[2];
};

void LoadMeshAsset(FileLoadThreadData* file_load_thread_data,
                   MeshAsset* mesh_asset, const char* path) 
{
    FBXParseScene parse_scene;
    LoadFBX(&parse_scene, path, file_load_thread_data, NULL);
    Mesh& mesh = parse_scene.meshes[0];
    RecalculateNormals(&mesh);
    mesh_asset->num_index = mesh.num_tris*3;
    GetBoundingBox(&mesh, mesh_asset->bounding_box);
    VBOFromMesh(&mesh, &mesh_asset->vert_vbo, &mesh_asset->index_vbo);
    parse_scene.Dispose();
}

void FillStaticDrawable(Drawable* drawable, const MeshAsset& mesh_asset, 
                        int texture, int shader, vec3 translation) 
{
    drawable->vert_vbo = mesh_asset.vert_vbo;
    drawable->index_vbo = mesh_asset.index_vbo;
    drawable->num_indices = mesh_asset.num_index;
    drawable->vbo_layout = kInterleave_3V2T3N;
    drawable->texture_id = texture;
    drawable->shader_id = shader;
    SeparableTransform sep_transform;
    sep_transform.translation = translation;
    drawable->transform = sep_transform.GetCombination();
}

void GameState::Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data) {
    MeshAsset fbx_lamp, fbx_floor, fbx_fountain, fbx_flowerbox, 
              fbx_garden_tall_corner, fbx_garden_tall_nook, fbx_garden_tall_stairs,
              fbx_garden_tall_wall, fbx_short_wall, fbx_wall_pillar, fbx_tree;
    LoadMeshAsset(file_load_thread_data, &fbx_lamp, 
                  asset_list[kFBXLamp]);
    LoadMeshAsset(file_load_thread_data, &fbx_fountain, 
                  asset_list[kFBXFountain]);
    LoadMeshAsset(file_load_thread_data, &fbx_flowerbox, 
                  asset_list[kFBXFlowerbox]);
    LoadMeshAsset(file_load_thread_data, &fbx_garden_tall_corner, 
                  asset_list[kFBXGardenTallCorner]);
    LoadMeshAsset(file_load_thread_data, &fbx_garden_tall_nook, 
                  asset_list[kFBXGardenTallNook]);
    LoadMeshAsset(file_load_thread_data, &fbx_garden_tall_stairs, 
                  asset_list[kFBXGardenTallStairs]);
    LoadMeshAsset(file_load_thread_data, &fbx_garden_tall_wall, 
                  asset_list[kFBXGardenTallWall]);
    LoadMeshAsset(file_load_thread_data, &fbx_short_wall, 
                  asset_list[kFBXShortWall]);
    LoadMeshAsset(file_load_thread_data, &fbx_wall_pillar, 
                  asset_list[kFBXTree]);
    LoadMeshAsset(file_load_thread_data, &fbx_wall_pillar, 
                  asset_list[kFBXWallPillar]);
    LoadMeshAsset(file_load_thread_data, &fbx_floor, 
                  asset_list[kFBXFloor]);
    LoadMeshAsset(file_load_thread_data, &fbx_tree, 
                  asset_list[kFBXTree]);

    profiler->StartEvent("Loading textures");
    int tex_lamp = 
        LoadImage(asset_list[kTexLamp], file_load_thread_data);
    int tex_fountain = 
        LoadImage(asset_list[kTexFountain], file_load_thread_data);
    int tex_flower_box = 
        LoadImage(asset_list[kTexFlowerbox], file_load_thread_data);
    int tex_garden_tall_corner = 
        LoadImage(asset_list[kTexGardenTallCorner], file_load_thread_data);
    int tex_garden_tall_nook = 
        LoadImage(asset_list[kTexGardenTallNook], file_load_thread_data);
    int tex_garden_tall_stairs = 
        LoadImage(asset_list[kTexGardenTallStairs], file_load_thread_data);
    int tex_garden_tall_wall = 
        LoadImage(asset_list[kTexGardenTallWall], file_load_thread_data);
    int tex_short_wall = 
        LoadImage(asset_list[kTexShortWall], file_load_thread_data);
    int tex_tree = 
        LoadImage(asset_list[kTexTree], file_load_thread_data);
    int tex_wall_pillar = 
        LoadImage(asset_list[kTexWallPillar], file_load_thread_data);
    int tex_floor = 
        LoadImage(asset_list[kTexFloor], file_load_thread_data);
    int tex_char = 
        LoadImage(asset_list[kTexChar], file_load_thread_data);
    profiler->EndEvent();

    profiler->StartEvent("Loading shaders");
    int shader_3d_model = 
        CreateProgramFromFile(file_load_thread_data, asset_list[kShader3DModel]);
    int shader_3d_model_skinned = 
        CreateProgramFromFile(file_load_thread_data, asset_list[kShader3DModelSkinned]);
    int shader_debug_draw = 
        CreateProgramFromFile(file_load_thread_data, asset_list[kShaderDebugDraw]);
    int shader_debug_draw_text = 
        CreateProgramFromFile(file_load_thread_data, asset_list[kShaderDebugDrawText]);
    profiler->EndEvent();

    camera.position = vec3(0.0f,0.0f,20.0f);
    camera.rotation_x = 0.0f;
    camera.rotation_y = 0.0f;

    editor_mode = false;

    lines.shader = shader_debug_draw;

    LoadTTF(asset_list[kFontDebug], &text_atlas, file_load_thread_data, 18.0f);
    text_atlas.shader = shader_debug_draw_text;
    text_atlas.vert_vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);
    text_atlas.index_vbo = CreateVBO(kElementVBO, kStreamVBO, NULL, 0);
    debug_text.Init(&text_atlas);

    lines.num_lines = 0;
    num_drawables = 0;
    FillStaticDrawable(&drawables[num_drawables++], fbx_lamp, tex_lamp,
                       shader_3d_model, vec3(0,0,2));

    lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);

    int character_vert_vbo, character_index_vbo, num_character_indices;
    {
        profiler->StartEvent("Parsing character fbx");
        profiler->StartEvent("Loading file to RAM");
        FBXParseScene parse_scene;
        void* mem = malloc(1024*1024*64);
        int len;
        char err_title[FileLoadThreadData::kMaxErrMsgLen];
        char err_msg[FileLoadThreadData::kMaxErrMsgLen];
        if(!FileLoadThreadData::LoadFile(asset_list[kFBXChar], mem, &len,
                                         err_title, err_msg) )
        {
            FormattedError(err_title, err_msg);
            exit(1);
        }
        profiler->EndEvent();

        profiler->StartEvent("Parsing character fbx");
        const char* names[] = {"RiggedMesh", "rig"};
        ParseFBXFromRAM(&parse_scene, mem, len, names, 2);
        Mesh& mesh = parse_scene.meshes[0];
        Skeleton& skeleton = parse_scene.skeletons[0];
        profiler->EndEvent();

        RecalculateNormals(&mesh);
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
            //mat4 rot_mat = toMat4(angleAxis(-glm::half_pi<float>(), vec3(1.0f,0.0f,0.0f)));
            character.local_bone_transforms[bone_index] = temp * inverse(bind_mat);// * rot_mat;
        }

        profiler->StartEvent("Creating VBO and adding to scene");
        vec3 char_bb[2];
        GetBoundingBox(&mesh, char_bb);
        VBOFromSkinnedMesh(&mesh, &character_vert_vbo, &character_index_vbo);
        num_character_indices = mesh.num_tris*3;

        char_drawable = num_drawables;
        drawables[num_drawables].vert_vbo = character_vert_vbo;
        drawables[num_drawables].index_vbo = character_index_vbo;
        drawables[num_drawables].num_indices = num_character_indices;
        drawables[num_drawables].vbo_layout = kInterleave_3V2T3N4I4W;
        SeparableTransform char_transform;
        char_transform.scale = vec3(1.0f);
        drawables[num_drawables].transform = mat4();//char_transform.GetCombination();
        drawables[num_drawables].texture_id = tex_char;
        drawables[num_drawables].shader_id = shader_3d_model_skinned;
        drawables[num_drawables].bone_transforms = character.display_bone_transforms;
        ++num_drawables;
        profiler->EndEvent();

        lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);
        parse_scene.Dispose();
        profiler->EndEvent();
    }

    FillStaticDrawable(&drawables[num_drawables++], fbx_tree, tex_tree,
        shader_3d_model, vec3(2,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_fountain, tex_fountain,
        shader_3d_model, vec3(4,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_flowerbox, tex_flower_box,
        shader_3d_model, vec3(6,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_corner, tex_garden_tall_corner,
        shader_3d_model, vec3(8,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_nook, tex_garden_tall_nook,
        shader_3d_model, vec3(10,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_wall, tex_garden_tall_wall,
        shader_3d_model, vec3(12,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_stairs, tex_garden_tall_stairs,
        shader_3d_model, vec3(14,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_short_wall, tex_short_wall,
        shader_3d_model, vec3(16,0,0));
    FillStaticDrawable(&drawables[num_drawables++], fbx_wall_pillar, tex_wall_pillar,
        shader_3d_model, vec3(18,0,0));

    for(int i=-10; i<10; ++i){
        for(int j=-10; j<10; ++j){
            FillStaticDrawable(&drawables[num_drawables++], fbx_floor, tex_floor,
                               shader_3d_model, vec3(j*2-1,0,i*2-1));
        }
    }
}

void Update(GameState* game_state, const vec2& mouse_rel, float time_step) {
    static const float char_speed = 2.0f;
    static const float char_accel = 10.0f;
    float cam_speed = 10.0f;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_SPACE]) {
        cam_speed *= 0.1f;
    }
    if(game_state->editor_mode){
        vec3 offset;
        if (state[SDL_SCANCODE_W]) {
            offset -= vec3(0,0,1);
        }
        if (state[SDL_SCANCODE_S]) {
            offset += vec3(0,0,1);
        }
        if (state[SDL_SCANCODE_A]) {
            offset -= vec3(1,0,0);
        }
        if (state[SDL_SCANCODE_D]) {
            offset += vec3(1,0,0);
        }
        game_state->camera.position += 
            game_state->camera.GetRotation() * offset * cam_speed * time_step;
        const float kMouseSensitivity = 0.003f;
        Uint32 mouse_button_bitmask = SDL_GetMouseState(NULL, NULL);
        if(mouse_button_bitmask & SDL_BUTTON_LEFT){
            game_state->camera.rotation_x -= mouse_rel.y * kMouseSensitivity;
            game_state->camera.rotation_y -= mouse_rel.x * kMouseSensitivity;
        }
        game_state->camera_fov = 1.02f;
    } else {
        game_state->camera.rotation_x = -0.75f;
        game_state->camera.rotation_y = 1.0f;

        vec3 temp = game_state->camera.GetRotation() * vec3(0,0,-1);
        vec3 cam_north = normalize(vec3(temp[0], 0.0f, temp[1]));
        vec3 cam_east = normalize(vec3(-cam_north[2], 0.0f, cam_north[0]));

        vec3 target_dir;
        float target_speed = 0.0f;
        if (state[SDL_SCANCODE_W]) {
            target_dir += cam_north;
        }
        if (state[SDL_SCANCODE_S]) {
            target_dir -= cam_north;
        }
        if (state[SDL_SCANCODE_D]) {
            target_dir += cam_east;
        }
        if (state[SDL_SCANCODE_A]) {
            target_dir -= cam_east;
        }

        if(length(target_dir) > 1.0f){
            target_dir = normalize(target_dir);
        }
        vec3 target_vel = target_dir * char_speed;
        if(time_step != 0.0f){
            vec3 rel_vel = target_vel - game_state->character.velocity;
            rel_vel /= (char_accel * time_step);
            if(length(rel_vel) > 1.0f){
                rel_vel = normalize(rel_vel);
            }
            rel_vel *= (char_accel * time_step);
            game_state->character.velocity += rel_vel;
        }
        game_state->character.transform.translation += 
            game_state->character.velocity * char_speed * time_step;


        static float char_rotation = 0.0f;
        static const float turn_speed = 10.0f;
        if(length(target_dir) > 0.0f){
            if(length(target_dir) > 1.0f){
                target_dir = normalize(target_dir);
            }
            float target_rotation = -atan2f(target_dir[2], target_dir[0])+half_pi<float>();

            float rel_rotation = target_rotation - char_rotation;
            // TODO: Do this in a better way, maybe using modf
            while(rel_rotation > pi<float>()){
                rel_rotation -= two_pi<float>();
            }
            while(rel_rotation < -pi<float>()){
                rel_rotation += two_pi<float>();
            }
            if(fabsf(rel_rotation) < turn_speed * time_step){
                char_rotation += rel_rotation;
            } else {
                char_rotation += (rel_rotation>0.0f?1.0f:-1.0f) * turn_speed * time_step;
            }
            game_state->character.transform.rotation = angleAxis(char_rotation, vec3(0,1,0)); 
        }

        game_state->camera.position = game_state->character.transform.translation +
            game_state->camera.GetRotation() * vec3(0,0,1) * 10.0f;
        game_state->camera_fov = 0.8f;
    }
    // TODO: we don't really want non-const static variables like this V
    static bool old_tab = false;
    if (state[SDL_SCANCODE_TAB] && !old_tab) {
        game_state->editor_mode = !game_state->editor_mode;
    }
    old_tab = (state[SDL_SCANCODE_TAB] != 0);
}

void DrawCoordinateGrid(GameState* game_state){
    static const float opac = 0.25f;
    static const vec4 basic_grid_color(1.0f, 1.0f, 1.0f, opac);
    static const vec4 x_axis_color(1.0f, 0.0f, 0.0f, opac);
    static const vec4 y_axis_color(0.0f, 1.0f, 0.0f, opac);
    static const vec4 z_axis_color(0.0f, 0.0f, 1.0f, opac);
    for(int i=-10; i<11; ++i){
        game_state->lines.Add(vec3(-10.0f, 0.0f, i), vec3(10.0f, 0.0f, i),
                              i==0?x_axis_color:basic_grid_color, kDraw, 1);
        game_state->lines.Add(vec3(i, 0.0f, -10.0f), vec3(i, 0.0f, 10.0f),
                              i==0?z_axis_color:basic_grid_color, kDraw, 1);
    }
    game_state->lines.Add(vec3(0.0f, -10.0f, 0.0f), vec3(0.0f, 10.0f,  0.0f),
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
        glUniformMatrix4fv(bone_transforms_uniform, 128, false, (GLfloat*)drawable->bone_transforms);
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
}

void Draw(GraphicsContext* context, GameState* game_state, int ticks) {
    CHECK_GL_ERROR();

    glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
    glClearColor(0.5,0.5,0.5,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat = glm::perspective(game_state->camera_fov, aspect_ratio, 0.1f, 100.0f);
    mat4 view_mat = inverse(game_state->camera.GetMatrix());
    mat4 proj_view_mat = proj_mat * view_mat;

    game_state->drawables[game_state->char_drawable].transform = 
        game_state->character.transform.GetCombination();

    for(int i=0; i<128; ++i){
        game_state->character.display_bone_transforms[i] = 
            game_state->drawables[game_state->char_drawable].transform * 
            game_state->character.local_bone_transforms[i];
    }

    for(int i=0; i<game_state->num_drawables; ++i){
        Drawable* drawable = &game_state->drawables[i];
        DrawDrawable(proj_view_mat, drawable);
    }

    static const bool draw_coordinate_grid = false;
    if(draw_coordinate_grid){
        DrawCoordinateGrid(game_state);
    }
    CHECK_GL_ERROR();
    game_state->lines.Draw(proj_view_mat);
    CHECK_GL_ERROR();
    game_state->debug_text.Draw(context);
    CHECK_GL_ERROR();
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

    GameState game_state;
    game_state.Init(&profiler, &file_load_thread_data);

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
        Draw(&graphics_context, &game_state, SDL_GetTicks());
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
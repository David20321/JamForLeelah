#include "game/game_state.h"
#include "game/nav_mesh.h"
#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/profiler.h"
#include "fbx/fbx.h"
#include "internal/common.h"
#include "internal/memory.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/norm.hpp"
#include "SDL.h"
#include "GL/glew.h"
#include "GL/gl.h"
#include <cstring>

using namespace glm;

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
    ASSET_PATH "art/main_character_rig_export.txt",
    ASSET_PATH "art/main_character_c.tga",
    ASSET_PATH "fonts/LiberationMono-Regular.ttf",
    ASSET_PATH "shaders/3D_model",
    ASSET_PATH "shaders/3D_model_skinned",
    ASSET_PATH "shaders/debug_draw",
    ASSET_PATH "shaders/debug_draw_text",
    ASSET_PATH "shaders/nav_mesh"
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
    kModelChar,
    kTexChar,
    kFontDebug,
    kShader3DModel,
    kShader3DModelSkinned,
    kShaderDebugDraw,
    kShaderDebugDrawText,
    kShaderNavMesh
};

static const bool kDrawNavMesh = false;

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

void GameState::Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data, StackAllocator* stack_allocator) {
    { // Allocate memory for debug lines
        int mem_needed = lines.AllocMemory(NULL);
        void* mem = stack_allocator->Alloc(mem_needed);
        if(!mem) {
            FormattedError("Error", "Could not allocate memory for DebugLines (%d bytes)", mem_needed);
        } else {
            lines.AllocMemory(mem);
        }
    }

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
    int shader_nav_mesh = 
        CreateProgramFromFile(file_load_thread_data, asset_list[kShaderNavMesh]);
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

    lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);

    num_character_assets = 0;
    {
        ParseMesh* parse_mesh = &character_assets[num_character_assets].parse_mesh;
        ParseTestFile(asset_list[kModelChar], parse_mesh);
        character_assets[num_character_assets].vert_vbo = 
            CreateVBO(kArrayVBO, kStaticVBO, parse_mesh->vert, 
                      parse_mesh->num_vert*sizeof(float)*16);
        character_assets[num_character_assets].index_vbo = 
            CreateVBO(kElementVBO, kStaticVBO, parse_mesh->indices, 
                      parse_mesh->num_index*sizeof(Uint32));
        for(int bone_index=0; bone_index<parse_mesh->num_bones; ++bone_index){
            character_assets[num_character_assets].bind_transforms[bone_index] = 
                parse_mesh->rest_mats[bone_index];
        }
        ++num_character_assets;
    }

    for(int i=0; i<kMaxCharacters; ++i){
        characters[i].exists = false;
    }

    for(int i=0; i<kMaxCharacters; ++i){
        characters[i].rotation = 0.0f;
        characters[i].exists = true;
        characters[i].nav_mesh_walker.tri = 0;
        characters[i].nav_mesh_walker.bary_pos = vec3(1/3.0f);
        characters[i].character_asset = &character_assets[0];
        characters[i].tether_target = -1;
        if(i == 0){
            characters[i].transform.translation = vec3(kMapSize,0,kMapSize);
            characters[i].mind.state = Mind::kPlayerControlled;
            characters[i].type = Character::kPlayer;
            characters[i].revealed = true;
        } else {
            characters[i].transform.translation = vec3(rand()%(kMapSize*2),0,rand()%(kMapSize*2));
            characters[i].mind.state = Mind::kWander;
            characters[i].mind.wander_update_time = 0;
            characters[i].type = (rand()%2==0)?Character::kRed:Character::kGreen;
            characters[i].revealed = false;
        }
        characters[i].energy = 1.0f;
        characters[i].walk_cycle_frame = (float)(rand()%100);

        char_drawable = num_drawables;
        drawables[num_drawables].vert_vbo = 
            characters[i].character_asset->vert_vbo;
        drawables[num_drawables].index_vbo = 
            characters[i].character_asset->index_vbo;
        drawables[num_drawables].num_indices = 
            characters[i].character_asset->parse_mesh.num_index;
        drawables[num_drawables].vbo_layout = kInterleave_3V2T3N4I4W;
        drawables[num_drawables].transform = mat4();
        drawables[num_drawables].texture_id = tex_char;
        drawables[num_drawables].shader_id = shader_3d_model_skinned;
        drawables[num_drawables].character = &characters[i];
        ++num_drawables;
    }

    // Initialize tile map
    for(int z=0; z<kMapSize; ++z){
        for(int j=0; j<kMapSize; ++j){
            tile_height[z*kMapSize+j] = 0;
        }
    }

    /*
    FillStaticDrawable(&drawables[num_drawables++], fbx_lamp, tex_lamp,
        shader_3d_model, vec3(0,0,2));
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
        shader_3d_model, vec3(18,0,0));*/
    nav_mesh.num_verts = 0;
    nav_mesh.num_indices = 0;

    for(int z=0; z<kMapSize; ++z){
        for(int x=0; x<kMapSize; ++x){
            vec3 translation(x*2,tile_height[z*kMapSize+x]*2,z*2);// Check nooks
            if(x<kMapSize-1 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)] && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_nook, tex_garden_tall_nook,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_nook, tex_garden_tall_nook,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_nook, tex_garden_tall_nook,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_nook, tex_garden_tall_nook,
                    shader_3d_model, translation);
            } 
            // Check walls
            else if(x<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_wall, tex_garden_tall_wall,
                    shader_3d_model, translation);
            } else if(x>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_wall, tex_garden_tall_wall,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_wall, tex_garden_tall_wall,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_wall, tex_garden_tall_wall,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            }
            // Check corners 
            else if(x<kMapSize-1 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_corner, tex_garden_tall_corner,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_corner, tex_garden_tall_corner,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_corner, tex_garden_tall_corner,
                    shader_3d_model, translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], fbx_garden_tall_corner, tex_garden_tall_corner,
                    shader_3d_model, translation);
            } // Basic floor
            else {
                FillStaticDrawable(&drawables[num_drawables++], fbx_floor, tex_floor,
                    shader_3d_model, translation);
                nav_mesh.verts[nav_mesh.num_verts++] = translation;
                nav_mesh.verts[nav_mesh.num_verts++] = translation + vec3(-2,0,0);
                nav_mesh.verts[nav_mesh.num_verts++] = translation + vec3(-2,0,2);
                nav_mesh.verts[nav_mesh.num_verts++] = translation + vec3(0,0,2);
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-4;
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-3;
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-2;
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-4;
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-2;
                nav_mesh.indices[nav_mesh.num_indices++] = nav_mesh.num_verts-1;
            }
        }
    }

    nav_mesh.vert_vbo = CreateVBO(kArrayVBO, kStaticVBO, nav_mesh.verts, nav_mesh.num_verts*sizeof(vec3));
    nav_mesh.index_vbo = CreateVBO(kElementVBO, kStaticVBO, nav_mesh.indices, nav_mesh.num_indices*sizeof(Uint32));
    nav_mesh.shader = shader_nav_mesh;
    if(kDrawNavMesh) {
        for(int i=0; i<nav_mesh.num_indices; i+=3){
            for(int j=0; j<3; ++j){
                lines.Add(nav_mesh.verts[nav_mesh.indices[i+j]] + vec3(0,0.1,0), 
                          nav_mesh.verts[nav_mesh.indices[i+(j+1)%3]] + vec3(0,0.1,0),
                          vec4(1), kPersistent, 1);        
            }
        }
    }
    nav_mesh.CalcNeighbors(stack_allocator);
}

static void UpdateCharacter(Character* character, vec3 target_dir, float time_step,
                            const NavMesh& nav) 
{
    static const float char_speed = 2.0f;
    static const float char_accel = 10.0f;
    vec3 target_vel = target_dir * char_speed;
    if(time_step != 0.0f){
        vec3 rel_vel = target_vel - character->velocity;
        rel_vel /= (char_accel * time_step);
        if(length(rel_vel) > 1.0f){
            rel_vel = normalize(rel_vel);
        }
        rel_vel *= (char_accel * time_step);
        character->velocity += rel_vel;
        SDL_assert(character->velocity == character->velocity);
    }
    character->transform.translation += 
        character->velocity * char_speed * time_step;

    {
        NavMeshWalker& walker = character->nav_mesh_walker;
        vec3 old_translation = character->transform.translation;
        vec3 pos = character->transform.translation;
        bool repeat;
        do {
            repeat = false;
            vec3 tri_verts[3];
            for(int i=0; i<3; ++i){
                tri_verts[i] = nav.verts[nav.indices[walker.tri*3+i]];
            }
            int tri_history[] = {walker.tri, walker.tri};
            vec3 tri_normal = cross(tri_verts[2] - tri_verts[0], tri_verts[1] - tri_verts[0]);
            for(int i=0; i<3; ++i){
                vec3 plane_n = normalize(cross(tri_verts[(i+1)%3] - tri_verts[(i+2)%3], tri_normal));
                float plane_d = dot(tri_verts[(i+1)%3], plane_n);
                float char_d = dot(pos, plane_n);
                if(char_d > plane_d){
                    int neighbor = nav.tri_neighbors[walker.tri*3+(i+1)%3];
                    if(neighbor != -1){
                        // Go to neighboring triangle if possible
                        tri_history[1] = tri_history[0];
                        tri_history[0] = walker.tri;
                        walker.tri = neighbor/3;
                        // Prevent infinite loops
                        if(walker.tri != tri_history[0] && walker.tri != tri_history[1]){
                            repeat = true;
                        }
                        break;
                    } else {
                        // Otherwise slide along wall
                        pos -= plane_n * (char_d - plane_d);
                        float char_vel_d = dot(character->velocity, plane_n);
                        if(char_vel_d > 0.0f){
                            character->velocity -= plane_n * (char_vel_d);
                            SDL_assert(character->velocity == character->velocity);
                        }
                    }
                }
            }
        } while(repeat);

        character->transform.translation = pos;
    }

    float walk_anim_speed = 30.0f;
    character->walk_cycle_frame += length(character->velocity) * walk_anim_speed * time_step;
    // TODO: this could be done much more elegantly using modf or similar
    while((int)character->walk_cycle_frame > Character::kWalkCycleEnd){
        character->walk_cycle_frame -=
            (float)(Character::kWalkCycleEnd - Character::kWalkCycleStart);
    }
    while((int)character->walk_cycle_frame < Character::kWalkCycleStart){
        character->walk_cycle_frame +=
            (float)(Character::kWalkCycleEnd - Character::kWalkCycleStart);
    }

    target_dir = character->velocity;

    static const float turn_speed = 10.0f;
    if(length(target_dir) > 0.0f){
        if(length(target_dir) > 1.0f){
            target_dir = normalize(target_dir);
        }
        float target_rotation = -atan2f(target_dir[2], target_dir[0])+half_pi<float>();

        float rel_rotation = target_rotation - character->rotation;
        // TODO: Do this in a better way, maybe using modf
        while(rel_rotation > pi<float>()){
            rel_rotation -= two_pi<float>();
        }
        while(rel_rotation < -pi<float>()){
            rel_rotation += two_pi<float>();
        }
        if(fabsf(rel_rotation) < turn_speed * time_step){
            character->rotation += rel_rotation;
        } else {
            character->rotation += (rel_rotation>0.0f?1.0f:-1.0f) * turn_speed * time_step;
        }
        character->transform.rotation = angleAxis(character->rotation, vec3(0,1,0)); 
    }

    character->color = vec4(1,1,1,character->energy);
    if(character->revealed){
        switch(character->type){
            case Character::kRed:
                character->color = vec4(1,0,0,character->energy);
                break;
            case Character::kGreen:
                character->color = vec4(0,1,0,character->energy);
                break;
        }
    }

    static const float kPlayerEnergyLossPerSecond = 0.01f;
    if(character->type == Character::kPlayer){
        character->energy -= time_step * kPlayerEnergyLossPerSecond;
    }
}


// Adapted from http://paulbourke.net/geometry/pointlineplane/source.c
vec3 ClosestPointOnSegment( const vec3& point, const vec3& segment_start, 
                            const vec3& segment_end) 
{
    vec3 segment_start_to_point = point - segment_start;
    vec3 segment_start_to_end   = segment_end - segment_start;
    float t = dot(segment_start_to_point, segment_start_to_end) / 
              distance2(segment_end, segment_start);
    t = clamp(t,0.0f,1.0f);
    return segment_start + t * segment_start_to_end;
}


void GameState::Update(const vec2& mouse_rel, float time_step) {
    lines.Update();
    float cam_speed = 10.0f;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_SPACE]) {
        cam_speed *= 0.1f;
    }
    if(editor_mode){
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
        camera.position += 
            camera.GetRotation() * offset * cam_speed * time_step;
        const float kMouseSensitivity = 0.003f;
        Uint32 mouse_button_bitmask = SDL_GetMouseState(NULL, NULL);
        if(mouse_button_bitmask & SDL_BUTTON_LEFT){
            camera.rotation_x -= mouse_rel.y * kMouseSensitivity;
            camera.rotation_y -= mouse_rel.x * kMouseSensitivity;
        }
        camera_fov = 1.02f;
    } else {
        camera.rotation_x = -0.75f;
        camera.rotation_y = 1.0f;

        vec3 temp = camera.GetRotation() * vec3(0,0,-1);
        vec3 cam_north = normalize(vec3(temp[0], 0.0f, temp[1]));
        vec3 cam_east = normalize(vec3(-cam_north[2], 0.0f, cam_north[0]));

        vec3 controls_target_dir;
        float target_speed = 0.0f;
        if (state[SDL_SCANCODE_W]) {
            controls_target_dir += cam_north;
        }
        if (state[SDL_SCANCODE_S]) {
            controls_target_dir -= cam_north;
        }
        if (state[SDL_SCANCODE_D]) {
            controls_target_dir += cam_east;
        }
        if (state[SDL_SCANCODE_A]) {
            controls_target_dir -= cam_east;
        }

        if(length2(controls_target_dir) > 1.0f){
            controls_target_dir = normalize(controls_target_dir);
        }

        int get_ticks = SDL_GetTicks();
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists){
                SDL_assert(characters[i].transform.translation == characters[i].transform.translation);
                vec3 target_dir;
                switch(characters[i].mind.state){
                case Mind::kPlayerControlled:
                    target_dir = controls_target_dir;
                    break;
                case Mind::kStand:
                    target_dir = vec3(0.0f);
                    break;
                case Mind::kWander:
                    if(get_ticks > characters[i].mind.wander_update_time){
                        vec2 rand_dir = glm::circularRand(glm::linearRand(0.0f,0.5f));
                        characters[i].mind.dir = vec3(rand_dir[0], 0.0f, rand_dir[1]);
                        characters[i].mind.wander_update_time = get_ticks + glm::linearRand<int>(1000,5000);
                    }
                    target_dir = characters[i].mind.dir;
                    break;
                case Mind::kSeekTarget:
                case Mind::kAvoidTarget:
                    if(characters[characters[i].mind.seek_target].exists){
                        target_dir = characters[i].transform.translation - 
                            characters[characters[i].mind.seek_target].transform.translation;
                        if(characters[i].mind.state == Mind::kSeekTarget){
                            target_dir *= -1.0f;
                        }
                        float target_dir_len = length(target_dir);
                        if(target_dir_len > characters[i].mind.seek_target_distance &&
                           target_dir_len > 0.001f)
                        {
                            target_dir = normalize(target_dir) * 0.5f;
                        } else {
                            target_dir = vec3(0.0f);
                        }
                        SDL_assert(target_dir == target_dir);
                    } else {
                        target_dir = vec3(0.0f);
                    }
                    break;
                }
                UpdateCharacter(&characters[i], target_dir, time_step, nav_mesh);
            }
        }

        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists && characters[i].tether_target != -1 && characters[characters[i].tether_target].exists){
                vec3 height_vec = vec3(0.0f,0.5f,0.0f);
                float closest_dist = FLT_MAX;
                int closest_tether = -1;
                for(int j=0; j<kMaxCharacters; ++j){
                    if(j != i){
                        vec3 pos = characters[j].transform.translation;
                        vec3 point = ClosestPointOnSegment(pos,
                            characters[i].transform.translation,
                            characters[characters[i].tether_target].transform.translation);
                        if(distance2(point, pos) < 0.4f*0.4f) {
                            float dist = distance2(characters[i].transform.translation, pos);
                            if(dist < closest_dist){
                                closest_dist = dist;
                                closest_tether = j;
                            }
                        }
                    }
                }
                if(closest_tether != -1){
                    characters[i].tether_target = closest_tether;
                }                
                lines.Add(characters[i].transform.translation + height_vec, 
                          characters[characters[i].tether_target].transform.translation + height_vec,
                          vec4(0,1,0,characters[i].energy), kUpdate, 1);

                float player_missing_energy = 1.0f-characters[characters[i].tether_target].energy;
                float amount_transferred = min(player_missing_energy, time_step);
                characters[i].energy -= amount_transferred;
                characters[characters[i].tether_target].energy += amount_transferred;
            }
        }

        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].energy < 0.0f){
                characters[i].exists = false;
            }
        }

        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists && characters[i].mind.state == Mind::kPlayerControlled){
                camera.position = characters[i].transform.translation +
                    camera.GetRotation() * vec3(0,0,1) * 10.0f;
            }
        }
        CharacterCollisions(characters, time_step);
        camera_fov = 0.8f;
    }
    // TODO: we don't really want non-const static variables like this V
    static bool old_tab = false;
    if (state[SDL_SCANCODE_TAB] && !old_tab) {
        editor_mode = !editor_mode;
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

void DrawDrawable(const mat4 &proj_mat, const mat4 &view_mat, Drawable* drawable) {
    CHECK_GL_ERROR();
    glUseProgram(drawable->shader_id);
    CHECK_GL_ERROR();

    GLuint modelview_matrix_uniform = glGetUniformLocation(drawable->shader_id, "mv_mat");
    GLuint projection_matrix_uniform = glGetUniformLocation(drawable->shader_id, "proj_mat");
    GLuint normal_matrix_uniform = glGetUniformLocation(drawable->shader_id, "norm_mat");

    mat4 model_mat = drawable->transform;
    mat4 modelview_mat = view_mat * model_mat;
    mat3 normal_mat = mat3(model_mat);
    glUniformMatrix3fv(normal_matrix_uniform, 1, false, (GLfloat*)&normal_mat);

    GLuint texture_uniform = glGetUniformLocation(drawable->shader_id, "texture_id");
    glUniform1i(texture_uniform, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawable->texture_id);

    glBindBuffer(GL_ARRAY_BUFFER, drawable->vert_vbo);
    switch(drawable->vbo_layout){
    case kSimple_4V:
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&modelview_mat);
        glUniformMatrix4fv(projection_matrix_uniform, 1, false, (GLfloat*)&proj_mat);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
        glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(0);
        break;
    case kInterleave_3V2T3N:
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&modelview_mat);
        glUniformMatrix4fv(projection_matrix_uniform, 1, false, (GLfloat*)&proj_mat);
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
        SDL_assert(drawable->character != NULL);
        Character* character = drawable->character;
        drawable->transform = character->transform.GetCombination();
        ParseMesh* parse_mesh = &character->character_asset->parse_mesh;
        int animation = 1;//1;
        int frame = (int)character->walk_cycle_frame;
        int start_anim_transform = 
            parse_mesh->animations[animation].anim_transform_start +
            parse_mesh->num_bones * frame;
        mat4 bone_transforms[128];
        for(int bone_index=0; bone_index<parse_mesh->num_bones; ++bone_index){
            mat4 temp = parse_mesh->anim_transforms[start_anim_transform + bone_index];
            bone_transforms[bone_index] = temp * 
                inverse(parse_mesh->rest_mats[bone_index]);
        }
        for(int i=0; i<128; ++i){
            bone_transforms[i] = drawable->transform * bone_transforms[i];
        }
        GLuint color_uniform = glGetUniformLocation(drawable->shader_id, "color");
        glUniform4fv(color_uniform, 1, (GLfloat*)&character->color);
        glUniformMatrix4fv(projection_matrix_uniform, 1, false, (GLfloat*)&proj_mat);
        glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&view_mat);
        GLuint bone_transforms_uniform = glGetUniformLocation(drawable->shader_id, "bone_matrices");
        glUniformMatrix4fv(bone_transforms_uniform, 128, false, (GLfloat*)bone_transforms);
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

void GameState::Draw(GraphicsContext* context, int ticks) {
    CHECK_GL_ERROR();

    glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
    glClearColor(0.5,0.5,0.5,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat = glm::perspective(camera_fov, aspect_ratio, 0.1f, 100.0f);
    mat4 view_mat = inverse(camera.GetMatrix());

    for(int i=0; i<num_drawables; ++i){
        Drawable* drawable = &drawables[i];
        CHECK_GL_ERROR();
        DrawDrawable(proj_mat, view_mat, drawable);
        CHECK_GL_ERROR();
    }

    static const bool draw_coordinate_grid = false;
    if(draw_coordinate_grid){
        CHECK_GL_ERROR();
        DrawCoordinateGrid(this);
        CHECK_GL_ERROR();
    }
    if(kDrawNavMesh){
        nav_mesh.Draw(proj_mat * view_mat);
        CHECK_GL_ERROR();
    }
    lines.Draw(proj_mat * view_mat);
    CHECK_GL_ERROR();
    debug_text.Draw(context, ticks/1000.0f);
    CHECK_GL_ERROR();
}

void GameState::CharacterCollisions(Character* characters, float time_step) {
    // TODO: this is O(n^2), divide into grid or something
    static const float kCollideDist = 0.7f;
    static const float kCollideDist2 = kCollideDist * kCollideDist;
    for(int i=0; i<kMaxCharacters; ++i){
        for(int j=0; j<kMaxCharacters; ++j){
            vec3 *translation[] = {&characters[i].transform.translation, 
                                   &characters[j].transform.translation};
            Character* chars[] = {&characters[i], &characters[j]};
            int char_ids[] = {i, j};
            if(i!=j && chars[0]->exists && chars[1]->exists &&
               distance2(*translation[0], *translation[1]) < kCollideDist2)
            {
                vec3 mid = (*translation[0]+*translation[1]) * 0.5f;
                vec3 dir = *translation[1]-*translation[0];
                if(length2(dir) > 0.01f){
                    dir = normalize(dir);
                } else {
                    vec2 circle = glm::circularRand(1.0f);
                    dir = vec3(circle[0], 0.0f, circle[1]);
                }
                vec3 new_translation[2];
                new_translation[0] = mid - dir * kCollideDist * 0.5f;
                new_translation[1] = mid + dir * kCollideDist * 0.5f;
                if(time_step != 0.0f){
                    characters[i].velocity += (new_translation[0] - *translation[0])/time_step;
                    characters[j].velocity += (new_translation[1] - *translation[1])/time_step;
                }
                *translation[0] = new_translation[0];
                *translation[1] = new_translation[1];
                for(int k=0; k<2; ++k){
                    if(chars[k]->type == Character::kPlayer){
                        Character* other = chars[1-k];
                        if(!other->revealed){
                            other->revealed = true;
                            if(other->type == Character::kRed){
                                other->mind.state = Mind::kSeekTarget;
                                other->mind.seek_target = char_ids[k];
                                other->mind.seek_target_distance = 0.0f;
                            } else if(other->type == Character::kGreen){
                                other->tether_target = char_ids[k];
                                other->mind.state = Mind::kSeekTarget;
                                other->mind.seek_target = char_ids[k];
                                other->mind.seek_target_distance = 2.0f;
                            }
                        }
                        switch(other->type){
                        case Character::kRed:
                            other->energy -= time_step;
                            chars[k]->energy -= time_step;
                            break;
                        }
                    }
                    if(chars[k]->type == Character::kRed && chars[k]->revealed &&
                        chars[1-k]->type == Character::kGreen && chars[1-k]->revealed)
                    {
                        chars[k]->energy -= time_step;
                        chars[1-k]->energy -= time_step;
                    }                    
                }
            }
        }
    }
}

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
#include "platform_sdl/blender_file_io.h"
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
    kFBXChar,
    kTexChar,
    kFontDebug,
    kShader3DModel,
    kShader3DModelSkinned,
    kShaderDebugDraw,
    kShaderDebugDrawText,
    kShaderNavMesh
};

using namespace glm;

class ParseMesh;

class NavMesh {
public:
    static const int kMaxNavMeshVerts = 10000;
    static const int kMaxNavMeshTris = 10000;
    int num_verts;
    vec3 verts[kMaxNavMeshVerts];
    int num_indices;
    Uint32 indices[kMaxNavMeshTris*3];
    int vert_vbo;
    int index_vbo;
    int shader;
    int tri_neighbors[kMaxNavMeshTris*3];

    void CalcNeighbors(StackMemoryBlock* stack_allocator);
    void Draw(const mat4& proj_view_mat);
};

class NavMeshWalker {
public:
    int tri;
    vec3 bary_pos; //barycentric position
    vec3 GetWorldPos(NavMesh* nav_mesh);
    void ApplyWorldSpaceTranslation(NavMesh* nav_mesh, vec3 translation);
    vec3 GetBaryPos(NavMesh* nav_mesh, vec3 pos);
};

glm::vec3 NavMeshWalker::GetWorldPos(NavMesh* nav_mesh) {
    vec3 pos;
    for(int i=0; i<3; ++i){
        pos += nav_mesh->verts[nav_mesh->indices[tri*3+i]] * bary_pos[i];
    }
    return pos;
}

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
// Transcribed from Christer Ericson's Real-Time Collision Detection
// http://gamedev.stackexchange.com/a/23745
void Barycentric(vec3 p, vec3 a, vec3 b, vec3 c, vec3* bary)
{
    vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    (*bary)[1] = (d11 * d20 - d01 * d21) / denom;
    (*bary)[2] = (d00 * d21 - d01 * d20) / denom;
    (*bary)[0] = 1.0f - (*bary)[1] - (*bary)[2];
}

vec3 NavMeshWalker::GetBaryPos(NavMesh* nav_mesh, vec3 pos) {
    vec3 bary;
    Barycentric(pos, 
                nav_mesh->verts[nav_mesh->indices[tri*3+0]],
                nav_mesh->verts[nav_mesh->indices[tri*3+1]],
                nav_mesh->verts[nav_mesh->indices[tri*3+2]],
                &bary);
    return bary;
}

void NavMesh::Draw(const mat4& proj_view_mat) {
    glBindBuffer(GL_ARRAY_BUFFER, vert_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_vbo);
    glUseProgram(shader);
    GLuint modelview_matrix_uniform = glGetUniformLocation(shader, "mv_mat");
    glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&proj_view_mat);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

struct Vec3EdgeHash {
    int hash;
    int tri_index;
    vec3 verts[2];
};

int Vec3EdgeHashSort(const void* a_ptr, const void* b_ptr){
    const Vec3EdgeHash* a = (const Vec3EdgeHash*)a_ptr;
    const Vec3EdgeHash* b = (const Vec3EdgeHash*)b_ptr;
    if(a->hash < b->hash) {
        return -1;
    } else if(a->hash == b->hash){
        return 0;
    } else {
        return 1;
    }
}

void NavMesh::CalcNeighbors(StackMemoryBlock* stack_allocator) {
    //TODO: just compare each vec3 value in turn instead of using hashes, no need to risk collisions if don't have to
    static const int kMaxEdges = 10000;
    int num_unique_edges = 0;
    Vec3EdgeHash* unique_verts = (Vec3EdgeHash*)stack_allocator->Alloc(
        sizeof(Vec3EdgeHash)*kMaxEdges);
    if(!unique_verts){
        FormattedError("Error", "Could not allocate memory for Navmesh::CalcNeighbors unique_verts");
    }
    for(int i=0; i<num_indices; i+=3){
        for(int j=0; j<3; ++j){
            tri_neighbors[i+j] = -1;
            vec3 edge_verts[2] = {verts[indices[i+j]], verts[indices[i+(j+1)%3]]};
            // Make sure that identical edges have verts in the same order
            int vec3_hash[2];
            for(int k=0; k<2; ++k){
                vec3_hash[k] = djb2_hash_len((unsigned char*)&edge_verts[k], sizeof(vec3));
            }
            if(vec3_hash[0] > vec3_hash[1]){
                vec3 temp = edge_verts[0];
                edge_verts[0] = edge_verts[1];
                edge_verts[1] = temp;
            }
            unique_verts[num_unique_edges].hash = djb2_hash_len((unsigned char*)edge_verts, sizeof(vec3)*2);
            for(int k=0; k<2; ++k){
                unique_verts[num_unique_edges].verts[k] = edge_verts[k];
            }
            unique_verts[num_unique_edges].hash = djb2_hash_len((unsigned char*)edge_verts, sizeof(vec3)*2);
            unique_verts[num_unique_edges].tri_index = i+j;
            ++num_unique_edges;
            if(num_unique_edges >= kMaxEdges) {
                FormattedError("Error", "Too many edges for CalcNeighbors");
                exit(1);
            }
        }
    }
    qsort(unique_verts, num_unique_edges, sizeof(Vec3EdgeHash), Vec3EdgeHashSort);
    for(int i=1; i<num_unique_edges; ++i){
        if(unique_verts[i].hash == unique_verts[i-1].hash){
            if(unique_verts[i].verts[0] != unique_verts[i].verts[0] ||
                unique_verts[i].verts[1] != unique_verts[i].verts[1])
            {
                FormattedError("Error", "Hash collision in CalcNeighbors");
                exit(1);
            }
            tri_neighbors[unique_verts[i].tri_index] = unique_verts[i-1].tri_index;
            tri_neighbors[unique_verts[i-1].tri_index] = unique_verts[i].tri_index;
        }
    }
    stack_allocator->Free();
}

struct Character {
    vec3 velocity;
    SeparableTransform transform;
    mat4 display_bone_transforms[128];
    mat4 bind_transforms[128];
    mat4 local_bone_transforms[128];
    NavMeshWalker nav_mesh_walker;
    ParseMesh* parse_mesh;
    static const int kWalkCycleStart = 31;
    static const int kWalkCycleEnd = 58;
    float walk_cycle_frame;
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
    NavMesh nav_mesh;

    static const int kMapSize = 30;
    int tile_height[kMapSize * kMapSize];

    void Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data, StackMemoryBlock* stack_allocator);
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

void GameState::Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data, StackMemoryBlock* stack_allocator) {
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

    int character_vert_vbo, character_index_vbo, num_character_indices;
    {
        character.nav_mesh_walker.tri = 0;
        character.nav_mesh_walker.bary_pos = vec3(1/3.0f);

        character.parse_mesh = (ParseMesh*)malloc(sizeof(ParseMesh));
        ParseTestFile(ASSET_PATH "art/main_character_rig_export.txt", character.parse_mesh);
        character_vert_vbo = CreateVBO(kArrayVBO, kStaticVBO, character.parse_mesh->vert, character.parse_mesh->num_vert*sizeof(float)*16);
        character_index_vbo = CreateVBO(kElementVBO, kStaticVBO, character.parse_mesh->indices, character.parse_mesh->num_index*sizeof(Uint32));
        num_character_indices = character.parse_mesh->num_index;
        for(int bone_index=0; bone_index<character.parse_mesh->num_bones; ++bone_index){
            character.bind_transforms[bone_index] = character.parse_mesh->rest_mats[bone_index];
        }

        char_drawable = num_drawables;
        drawables[num_drawables].vert_vbo = character_vert_vbo;
        drawables[num_drawables].index_vbo = character_index_vbo;
        drawables[num_drawables].num_indices = num_character_indices;
        drawables[num_drawables].vbo_layout = kInterleave_3V2T3N4I4W;
        drawables[num_drawables].transform = mat4();
        drawables[num_drawables].texture_id = tex_char;
        drawables[num_drawables].shader_id = shader_3d_model_skinned;
        drawables[num_drawables].bone_transforms = character.display_bone_transforms;
        ++num_drawables;
        profiler->EndEvent();

        lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 0);
        profiler->EndEvent();
    }

    for(int z=0; z<kMapSize; ++z){
        for(int j=0; j<kMapSize; ++j){
            tile_height[z*kMapSize+j] = (rand()%20)==0;
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
    for(int i=0; i<nav_mesh.num_indices; i+=3){
        for(int j=0; j<3; ++j){
            lines.Add(nav_mesh.verts[nav_mesh.indices[i+j]] + vec3(0,0.1,0), 
                      nav_mesh.verts[nav_mesh.indices[i+(j+1)%3]] + vec3(0,0.1,0),
                      vec4(1), kPersistent, 1);        
        }
    }
    nav_mesh.CalcNeighbors(stack_allocator);
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

        vec3 bary_pos = 
            game_state->character.nav_mesh_walker.GetBaryPos(
                &game_state->nav_mesh,
                game_state->character.transform.translation);
        
        for(int i=0; i<3; ++i){
            bary_pos[i] = min(1.0f, max(0.0f, bary_pos[i])); 
        }
        float total_bary = 0.0f;
        for(int i=0; i<3; ++i){
            total_bary += bary_pos[i];
        }
        if(total_bary > 0.0f){
            for(int i=0; i<3; ++i){
                bary_pos[i] /= total_bary; 
            }
        }

        game_state->character.nav_mesh_walker.bary_pos = bary_pos;

        game_state->character.transform.translation = 
            game_state->character.nav_mesh_walker.GetWorldPos(&game_state->nav_mesh);

        float walk_anim_speed = 30.0f;
        game_state->character.walk_cycle_frame += length(game_state->character.velocity) * walk_anim_speed * time_step;
        // TODO: this could be done much more elegantly using modf or similar
        while((int)game_state->character.walk_cycle_frame > Character::kWalkCycleEnd){
            game_state->character.walk_cycle_frame -=
                (float)(Character::kWalkCycleEnd - Character::kWalkCycleStart);
        }
        while((int)game_state->character.walk_cycle_frame < Character::kWalkCycleStart){
            game_state->character.walk_cycle_frame +=
                (float)(Character::kWalkCycleEnd - Character::kWalkCycleStart);
        }

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

    ParseMesh* parse_mesh = game_state->character.parse_mesh;
    int animation = 1;//1;
    int frame = (int)game_state->character.walk_cycle_frame;
    int start_anim_transform = parse_mesh->animations[animation].anim_transform_start +
                               parse_mesh->num_bones * frame;
    for(int bone_index=0; bone_index<parse_mesh->num_bones; ++bone_index){
        mat4 temp = parse_mesh->anim_transforms[start_anim_transform + bone_index];
        static const bool debug_draw_skeleton = false;
        if(debug_draw_skeleton) {
            static const float size = 0.03f;
            game_state->lines.Add(vec3(temp*vec4(0,0,size,1.0f)), vec3(temp*vec4(0,0,-size,1.0f)), vec4(1.0f), kDraw, 1);
            game_state->lines.Add(vec3(temp*vec4(0,size,0,1.0f)), vec3(temp*vec4(0,-size,0,1.0f)), vec4(1.0f), kDraw, 1);
            game_state->lines.Add(vec3(temp*vec4(size,0,0,1.0f)), vec3(temp*vec4(-size,0,0,1.0f)), vec4(1.0f), kDraw, 1);
        }
        game_state->character.local_bone_transforms[bone_index] = temp * 
            inverse(parse_mesh->rest_mats[bone_index]);
    }
    
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
    game_state->nav_mesh.Draw(proj_view_mat);
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

    srand((unsigned)SDL_GetPerformanceCounter());

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
    game_state.Init(&profiler, &file_load_thread_data, &stack_memory_block);

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
        profiler.StartEvent("Update");
        float time_scale = 1.0f;// 0.1f;
        int ticks = SDL_GetTicks();
        Update(&game_state, mouse_rel, (ticks - last_ticks) / 1000.0f * time_scale);
        last_ticks = ticks;
        profiler.EndEvent();
        profiler.StartEvent("Draw");
        Draw(&graphics_context, &game_state, SDL_GetTicks());
        profiler.EndEvent();
        profiler.StartEvent("Audio");
        UpdateAudio(&audio_context);
        profiler.EndEvent();
        profiler.StartEvent("Swap");
        SDL_GL_SwapWindow(graphics_context.window);
        profiler.EndEvent();
        profiler.EndEvent();
    }
    
    game_state.character.parse_mesh->Dispose();
    free(game_state.character.parse_mesh);
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
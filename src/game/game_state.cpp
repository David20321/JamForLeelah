#include "game/game_state.h"
#include "game/nav_mesh.h"
#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/audio.h"
#include "platform_sdl/profiler.h"
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
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

using namespace glm;

const char* asset_list[] = {
    "start_static_draw_meshes",
    ASSET_PATH "art/street_lamp_export.txt",
    ASSET_PATH "art/dry_fountain_export.txt",
    ASSET_PATH "art/flower_box_export.txt",
    ASSET_PATH "art/garden_tall_corner_export.txt",
    ASSET_PATH "art/garden_tall_nook_export.txt",
    ASSET_PATH "art/garden_tall_stairs_export.txt",
    ASSET_PATH "art/short_wall_export.txt",
    ASSET_PATH "art/garden_tall_wall_export.txt",
    ASSET_PATH "art/tree_export.txt",
    ASSET_PATH "art/wall_pillar_export.txt",
    ASSET_PATH "art/floor_quad_export.txt",
    "end_static_draw_meshes",
    "start_nav_meshes",
    ASSET_PATH "art/garden_tall_wall_nav_export.txt",
    "end_nav_meshes",
    ASSET_PATH "art/main_character_rig_export.txt",
    "start_textures",
    ASSET_PATH "art/lamp_c.tga",
    ASSET_PATH "art/dry_fountain_c.tga",
    ASSET_PATH "art/flowerbox_c.tga",
    ASSET_PATH "art/garden_tall_corner_c.tga",
    ASSET_PATH "art/garden_tall_nook_c.tga",
    ASSET_PATH "art/garden_tall_stairs.tga",
    ASSET_PATH "art/garden_tall_wall_c.tga",
    ASSET_PATH "art/short_wall_c.tga",
    ASSET_PATH "art/tree_c.tga",
    ASSET_PATH "art/wall_pillar_c.tga",
    ASSET_PATH "art/tiling_cobbles_c.tga",
    ASSET_PATH "art/main_character_c.tga",
    "end_textures",
    "start_fonts",
    ASSET_PATH "fonts/LiberationMono-Regular.ttf",
    "end_fonts",
    "start_shaders",
    ASSET_PATH "shaders/3D_model",
    ASSET_PATH "shaders/3D_model_skinned",
    ASSET_PATH "shaders/debug_draw",
    ASSET_PATH "shaders/debug_draw_text",
    ASSET_PATH "shaders/nav_mesh",
    "end_shaders",
    "start_music",
    ASSET_PATH "music/UG - Layer 0 - Opt Drone.ogg",
    ASSET_PATH "music/UG - Pos Layer 1 - Base.ogg",
    ASSET_PATH "music/UG - Pos Layer 2 - Expanse.ogg",
    ASSET_PATH "music/UG - Neg Layer 1 - Base.ogg",
    ASSET_PATH "music/UG - Neg Layer 2 - Expanse.ogg",
    ASSET_PATH "music/UG - Neg Layer 3 - Isolation.ogg",
    ASSET_PATH "music/UG - Layer 4 - Drums.ogg",
    ASSET_PATH "music/UG - Layer 5 - Optional Drum.ogg",
    "end_music"
};

enum {
    kStartStaticDrawMeshes,
    kMeshLamp,
    kMeshFountain,
    kMeshFlowerbox,
    kMeshGardenTallCorner,
    kMeshGardenTallNook,
    kMeshGardenTallStairs,
    kMeshShortWall,
    kMeshGardenTallWall,
    kMeshTree,
    kMeshWallPillar,
    kMeshFloor,
    kEndStaticDrawMeshes,

    kStartNavMeshes,
    kNavGardenTallWall,
    kEndNavMeshes,

    kModelChar,
    
    kStartTextures,
    kTexLamp,
    kTexFountain,
    kTexFlowerbox,
    kTexGardenTallCorner,
    kTexGardenTallNook,
    kTexGardenTallStairs,
    kTexGardenTallWall,
    kTexShortWall,
    kTexTree,
    kTexWallPillar,
    kTexFloor,
    kTexChar,
    kEndTextures,

    kStartFonts,
    kFontDebug,
    kEndFonts,

    kStartShaders,
    kShader3DModel,
    kShader3DModelSkinned,
    kShaderDebugDraw,
    kShaderDebugDrawText,
    kShaderNavMesh,
    kEndShaders,

    kStartMusic,
    kOggDrone,
    kOggPosLayer1,
    kOggPosLayer2,
    kOggNegLayer1,
    kOggNegLayer2,
    kOggNegLayer3,
    kOggDrums1,
    kOggDrums2,
    kEndMusic
};

static const int kNumMesh = kEndStaticDrawMeshes-kStartStaticDrawMeshes-1;
int MeshID(int id){
    return id - kStartStaticDrawMeshes - 1;
}

static const int kNumTex = kEndTextures-kStartTextures-1;
int TexID(int id){
    return id - kStartTextures - 1;
}

static const int kNumShaders = kEndShaders-kStartShaders-1;
int ShaderID(int id){
    return id - kStartShaders - 1;
}

static const int kNumNavMesh = kEndNavMeshes-kStartNavMeshes-1;
int NavMeshID(int id){
    return id - kStartNavMeshes - 1;
}


static const bool kDrawNavMesh = true;

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

void LoadOgg(OggTrack* ogg_track, const char* path, FileLoadThreadData* file_load_data, StackAllocator* stack_alloc) {
    StartLoadFile(path, file_load_data);
    ogg_track->mem_len = file_load_data->memory_len;
    ogg_track->mem = stack_alloc->Alloc(file_load_data->memory_len);
    if(!ogg_track->mem){
        FormattedError("Error", "Failed to allocate memory for ogg file: %s", path);
    }
    memcpy(ogg_track->mem, file_load_data->memory, ogg_track->mem_len);
    EndLoadFile(file_load_data);
    ogg_track->vorbis_alloc.alloc_buffer_length_in_bytes = 200*1024; // Allocate 200 KB for vorbis decoding
    ogg_track->vorbis_alloc.alloc_buffer = (char*)stack_alloc->Alloc(
        ogg_track->vorbis_alloc.alloc_buffer_length_in_bytes);
    if(!ogg_track->vorbis_alloc.alloc_buffer) {
        FormattedError("Error", "Failed to allocate memory for ogg decoder for: %s", path);
    }
    int err;
    ogg_track->vorbis = stb_vorbis_open_memory((const unsigned char*)ogg_track->mem, 
                                               ogg_track->mem_len, 
                                               &err, 
                                               &ogg_track->vorbis_alloc);
    if(!ogg_track->vorbis){
        if(!ogg_track->vorbis_alloc.alloc_buffer) {
            FormattedError("Error", "Failed to create ogg decoder for: %s\nError code: %d", path, err);
        }
    }
    ogg_track->samples = stb_vorbis_stream_length_in_samples(ogg_track->vorbis);
    ogg_track->read_pos = 0;
    ogg_track->gain = 0.0f;
    ogg_track->target_gain = 0.0f;
    ogg_track->transition_speed = 0.0000003f;
    stb_vorbis_info info = stb_vorbis_get_info(ogg_track->vorbis);
    ogg_track->decoded = (float*)stack_alloc->Alloc(sizeof(float) * ogg_track->samples * info.channels);
    if(!ogg_track->decoded) {
        FormattedError("Error", "Failed to allocate memory for decoded ogg: %s", path);
    }
    stb_vorbis_get_samples_float_interleaved(ogg_track->vorbis, info.channels, ogg_track->decoded, ogg_track->samples * info.channels);

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

int CreateProgramFromFile(GraphicsContext* graphics_context, FileLoadThreadData* file_load_data, const char* path){
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
    int shader_id = graphics_context->num_shaders++;
    Shader* shader = &graphics_context->shaders[shader_id];
    if(graphics_context->num_shaders == GraphicsContext::kMaxShaders){
        FormattedError("Error", "Too many shaders");
        exit(1);
    }
    shader->gl_id = shader_program;
    for(int i=0; i<Shader::kNumUniformNames; ++i){
        shader->uniforms[i] = glGetUniformLocation(shader->gl_id, shader_uniform_names[i]);
    }
    return shader_id;
}

struct MeshAsset {
    int vert_vbo;
    int index_vbo;
    int num_index;
    vec3 bounding_box[2];
};

struct NavMeshAsset {
    int num_verts;
    vec3* verts;
    int num_indices;
    int* indices;
    ~NavMeshAsset();
};

NavMeshAsset::~NavMeshAsset() {
    SDL_assert(verts == NULL);
    SDL_assert(indices == NULL);
}

void LoadMeshAssetTxt(FileLoadThreadData* file_load_thread_data,
                      MeshAsset* mesh_asset, const char* path) 
{
    ParseMesh parse_mesh;
    ParseTestFile(path, &parse_mesh);
    mesh_asset->vert_vbo = 
        CreateVBO(kArrayVBO, kStaticVBO, parse_mesh.vert, 
        parse_mesh.num_vert*sizeof(float)*8);
    mesh_asset->index_vbo = 
        CreateVBO(kElementVBO, kStaticVBO, parse_mesh.indices, 
        parse_mesh.num_index*sizeof(Uint32));
    mesh_asset->num_index = parse_mesh.num_index;
    parse_mesh.Dispose();
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

void GameState::Init(GraphicsContext* graphics_context, AudioContext* audio_context, Profiler* profiler, FileLoadThreadData* file_load_thread_data, StackAllocator* stack_allocator) {
    num_ogg_tracks = 0;
    for(int i=kOggDrone; i<=kOggDrums2; ++i){
        if(num_ogg_tracks > kMaxOggTracks){
            FormattedError("Error", "Too many OggTracks");
            exit(1);
        }
        LoadOgg(&ogg_track[num_ogg_tracks++], asset_list[i], file_load_thread_data, stack_allocator);
    }
    for(int i=0; i<num_ogg_tracks; ++i) {
        audio_context->AddOggTrack(&ogg_track[i]);
    }

    { // Allocate memory for debug lines
        int mem_needed = lines.AllocMemory(NULL);
        void* mem = stack_allocator->Alloc(mem_needed);
        if(!mem) {
            FormattedError("Error", "Could not allocate memory for DebugLines (%d bytes)", mem_needed);
        } else {
            lines.AllocMemory(mem);
        }
    }

    MeshAsset mesh_assets[kNumMesh];
    for(int i=0; i<kNumMesh; ++i){
        LoadMeshAssetTxt(file_load_thread_data, &mesh_assets[i], 
            asset_list[kStartStaticDrawMeshes+i+1]);
    }

    NavMeshAsset nav_mesh_assets[kNumNavMesh];
    for(int i=0; i<kNumNavMesh; ++i){
        ParseMesh parse_mesh;
        ParseTestFile(asset_list[kStartNavMeshes+i+1], &parse_mesh);
        nav_mesh_assets[i].num_verts = parse_mesh.num_vert;
        nav_mesh_assets[i].num_indices = parse_mesh.num_index;
        nav_mesh_assets[i].verts = (vec3*)stack_allocator->Alloc(parse_mesh.num_vert*sizeof(vec3));
        if(!nav_mesh_assets[i].verts){
            FormattedError("Error","Failed to alloc memory for nav mesh asset verts");
            exit(1);
        }
        nav_mesh_assets[i].indices = (int*)stack_allocator->Alloc(parse_mesh.num_index*sizeof(int));
        if(!nav_mesh_assets[i].indices){
            FormattedError("Error","Failed to alloc memory for nav mesh asset indices");
            exit(1);
        }
        for(int j=0; j<parse_mesh.num_vert; ++j){
            for(int k=0; k<3; ++k){
                nav_mesh_assets[i].verts[j][k] = parse_mesh.vert[j*8+k];
            }
        }
        for(int j=0; j<parse_mesh.num_index; ++j){
            nav_mesh_assets[i].indices[j] = parse_mesh.indices[j];
        }
        parse_mesh.Dispose();
    }

    int textures[kNumTex];
    for(int i=0; i<kNumTex; ++i){
        textures[i] = LoadImage(asset_list[kStartTextures+i+1], file_load_thread_data);
    }

    int shaders[kNumShaders];
    for(int i=0; i<kNumShaders; ++i){
        shaders[i] = CreateProgramFromFile(graphics_context, file_load_thread_data, 
                                           asset_list[kStartShaders+i+1]);
    }
                  
    camera.position = vec3(0.0f,0.0f,20.0f);
    camera.rotation_x = 0.0f;
    camera.rotation_y = 0.0f;

    editor_mode = false;

    lines.shader = shaders[ShaderID(kShaderDebugDraw)];

    LoadTTF(asset_list[kFontDebug], &text_atlas, file_load_thread_data, 18.0f);
    text_atlas.shader = shaders[ShaderID(kShaderDebugDrawText)];
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
        ++num_character_assets;
    }

    for(int i=0; i<kMaxCharacters; ++i){
        characters[i].exists = false;
    }

    static const bool kOnlyOneCharacter = true;
    int num_chars = kOnlyOneCharacter?0:kMaxCharacters;

    for(int i=0; i<num_chars; ++i){
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

        characters[i].drawable = num_drawables;
        drawables[num_drawables].vert_vbo = 
            characters[i].character_asset->vert_vbo;
        drawables[num_drawables].index_vbo = 
            characters[i].character_asset->index_vbo;
        drawables[num_drawables].num_indices = 
            characters[i].character_asset->parse_mesh.num_index;
        drawables[num_drawables].vbo_layout = kInterleave_3V2T3N4I4W;
        drawables[num_drawables].transform = mat4();
        drawables[num_drawables].texture_id = textures[TexID(kTexChar)];
        drawables[num_drawables].shader_id = shaders[ShaderID(kShader3DModelSkinned)];;
        drawables[num_drawables].character = &characters[i];
        ++num_drawables;
    }

    // Initialize tile map
    for(int z=0; z<kMapSize; ++z){
        for(int j=0; j<kMapSize; ++j){
            tile_height[z*kMapSize+j] = rand()%20==0;
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
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallNook)], textures[TexID(kTexGardenTallNook)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallNook)], textures[TexID(kTexGardenTallNook)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallNook)], textures[TexID(kTexGardenTallNook)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallNook)], textures[TexID(kTexGardenTallNook)],
                    shaders[ShaderID(kShader3DModel)], translation);
            } 
            // Check walls
            else if(x<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallWall)], textures[TexID(kTexGardenTallWall)],
                    shaders[ShaderID(kShader3DModel)], translation);
            } else if(x>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallWall)], textures[TexID(kTexGardenTallWall)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallWall)], textures[TexID(kTexGardenTallWall)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallWall)], textures[TexID(kTexGardenTallWall)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                mat4 mat = transform.GetCombination();
                drawables[num_drawables-1].transform = mat;
                NavMeshAsset* nav_mesh_asset = &nav_mesh_assets[NavMeshID(kNavGardenTallWall)];
                int start_verts = nav_mesh.num_verts;
                for(int i=0; i<nav_mesh_asset->num_verts; ++i){
                    nav_mesh.verts[nav_mesh.num_verts++] = 
                        vec3(mat * vec4(nav_mesh_asset->verts[i], 1));
                }
                for(int i=0; i<nav_mesh_asset->num_indices; ++i){
                    nav_mesh.indices[nav_mesh.num_indices++] =
                        start_verts + nav_mesh_asset->indices[i];
                }
            }
            // Check corners 
            else if(x<kMapSize-1 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallCorner)], textures[TexID(kTexGardenTallCorner)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallCorner)], textures[TexID(kTexGardenTallCorner)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x-1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallCorner)], textures[TexID(kTexGardenTallCorner)],
                    shaders[ShaderID(kShader3DModel)], translation);
                SeparableTransform transform;
                transform.translation = translation + vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                drawables[num_drawables-1].transform = transform.GetCombination();
            } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x+1)]){
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshGardenTallCorner)], textures[TexID(kTexGardenTallCorner)],
                    shaders[ShaderID(kShader3DModel)], translation);
            } // Basic floor
            else {
                FillStaticDrawable(&drawables[num_drawables++], mesh_assets[MeshID(kMeshFloor)], textures[TexID(kTexFloor)],
                    shaders[ShaderID(kShader3DModel)], translation);
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
    nav_mesh.shader = shaders[ShaderID(kShaderNavMesh)];
    if(kDrawNavMesh) {
        for(int i=0; i<nav_mesh.num_indices; i+=3){
            for(int j=0; j<3; ++j){
                lines.Add(nav_mesh.verts[nav_mesh.indices[i+j]] + vec3(0,0.1,0), 
                          nav_mesh.verts[nav_mesh.indices[i+(j+1)%3]] + vec3(0,0.1,0),
                          vec4(1), kPersistent, 1);        
            }
        }
    }
    for(int i=kNumNavMesh-1; i>=0; --i){
        stack_allocator->Free(nav_mesh_assets[i].indices);
        nav_mesh_assets[i].indices = NULL;
        stack_allocator->Free(nav_mesh_assets[i].verts);
        nav_mesh_assets[i].verts = NULL;
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

int OggTrack(int val){
    return val - kOggDrone;
}

void GameState::Update(const vec2& mouse_rel, float time_step) {
    for(int i=0; i<num_ogg_tracks; ++i) {
        ogg_track[i].target_gain = 0.0f;
    }
    ogg_track[OggTrack(kOggDrone)].target_gain = 0.5f;

    lines.Update();
    float cam_speed = 10.0f;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_SPACE]) {
        cam_speed *= 0.1f;
    }
    if(editor_mode){
        ogg_track[OggTrack(kOggDrums2)].target_gain = 1.0f;
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
                // Check AI to get target movement
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

        // Handle tethering
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists && characters[i].tether_target != -1 && characters[characters[i].tether_target].exists){
                { // Characters can steal tethers if they are close to tether and closest to source
                    float closest_dist = FLT_MAX;
                    int closest_tether = -1;
                    for(int j=0; j<kMaxCharacters; ++j){
                        if(j != i && characters[j].exists){
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
                }
                // Draw tether
                vec3 height_vec = vec3(0.0f,0.5f,0.0f);
                lines.Add(characters[i].transform.translation + height_vec, 
                          characters[characters[i].tether_target].transform.translation + height_vec,
                          vec4(0,1,0,characters[i].energy), kUpdate, 1);
                // Transfer energy across tether if needed
                float player_missing_energy = 1.0f-characters[characters[i].tether_target].energy;
                float amount_transferred = min(player_missing_energy, time_step);
                characters[i].energy -= amount_transferred;
                characters[characters[i].tether_target].energy += amount_transferred;
            }
        }

        // Characters die if energy goes below zero
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].energy < 0.0f){
                characters[i].exists = false;
            }
        }


        bool player_alive = false;
        // Set camera to follow player
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists && characters[i].mind.state == Mind::kPlayerControlled){
                camera.position = characters[i].transform.translation +
                    camera.GetRotation() * vec3(0,0,1) * 10.0f;
                player_alive = true;
            }
        }

        int num_reds = 0;
        int num_greens = 0;
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists && characters[i].revealed){
                if(characters[i].type == Character::kRed){
                    ++num_reds;
                }
                if(characters[i].type == Character::kGreen){
                    ++num_greens;
                }
            }   
        }

        if(!player_alive){
            ogg_track[OggTrack(kOggNegLayer3)].target_gain = 1.0f;
        } else {
            ogg_track[OggTrack(kOggDrums1)].target_gain = min(1.0f, num_reds * 0.5f);
            float positive = (float)(num_greens - num_reds);
            if(positive > 0.0f){
                ogg_track[OggTrack(kOggPosLayer1)].target_gain = 1.0f;
                if(positive > 1.0f){
                    ogg_track[OggTrack(kOggPosLayer2)].target_gain = 1.0f;
                }
            } else {
                ogg_track[OggTrack(kOggNegLayer1)].target_gain = 1.0f;
                if(positive < -1.0f){
                    ogg_track[OggTrack(kOggNegLayer2)].target_gain = 1.0f;
                }
            }
        }

        // Handle collisions between characters
        // Includes revealing character colors and combat
        CharacterCollisions(characters, time_step);

        // Set rotation based on velocity
        for(int i=0; i<kMaxCharacters; ++i){
            if(characters[i].exists) {
                Character* character = &characters[i];
                vec3 target_dir = character->velocity;
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
            }
        }

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

void DrawDrawable(GraphicsContext* graphics_context, const mat4 &proj_mat, 
                  const mat4 &view_mat, Drawable* drawable, Profiler* profiler) 
{
    mat4 modelview_mat = view_mat * drawable->transform;
    { // TODO: fix this janky frustum culling (e.g. calc real bounding spheres)
        vec4 pos = proj_mat * modelview_mat * vec4(0,0,0,1);
        float bounding_sphere_radius = 6.0f;
        pos[3] += bounding_sphere_radius;
        if(pos[0] < -pos[3] || pos[0] >  pos[3] || pos[1] < -pos[3] || 
           pos[1] >  pos[3] || pos[2] < -pos[3] || pos[2] >  pos[3])
        {
            return;
        }
    }

    Shader *shader = &graphics_context->shaders[drawable->shader_id];
    CHECK_GL_ERROR();
    glUseProgram(shader->gl_id);
    CHECK_GL_ERROR();

    mat3 normal_mat = mat3(drawable->transform);
    glUniformMatrix3fv(shader->uniforms[Shader::kNormalMat3], 1, false, (GLfloat*)&normal_mat);

    glUniform1i(shader->uniforms[Shader::kTextureID], 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawable->texture_id);

    glBindBuffer(GL_ARRAY_BUFFER, drawable->vert_vbo);
    switch(drawable->vbo_layout){
    case kSimple_4V:
        glUniformMatrix4fv(shader->uniforms[Shader::kModelviewMat4], 1, false, (GLfloat*)&modelview_mat);
        glUniformMatrix4fv(shader->uniforms[Shader::kProjectionMat4], 1, false, (GLfloat*)&proj_mat);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->index_vbo);
        glDrawElements(GL_TRIANGLES, drawable->num_indices, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(0);
        break;
    case kInterleave_3V2T3N:
        glUniformMatrix4fv(shader->uniforms[Shader::kModelviewMat4], 1, false, (GLfloat*)&modelview_mat);
        glUniformMatrix4fv(shader->uniforms[Shader::kProjectionMat4], 1, false, (GLfloat*)&proj_mat);
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
        ParseMesh* parse_mesh = &character->character_asset->parse_mesh;
        int animation = 1;//1;
        int frame = (int)character->walk_cycle_frame;
        int start_anim_transform = 
            parse_mesh->animations[animation].anim_transform_start +
            parse_mesh->num_bones * frame;
        mat4 bone_transforms[128];
        for(int bone_index=0; bone_index<parse_mesh->num_bones; ++bone_index){
            mat4 temp = parse_mesh->anim_transforms[start_anim_transform + bone_index];
            bone_transforms[bone_index] = temp * parse_mesh->inverse_rest_mats[bone_index];
        }
        for(int i=0; i<parse_mesh->num_bones; ++i){
            bone_transforms[i] = drawable->transform * bone_transforms[i];
        }
        glUniform4fv(shader->uniforms[Shader::kColor], 1, (GLfloat*)&character->color);
        glUniformMatrix4fv(shader->uniforms[Shader::kProjectionMat4], 1, false, (GLfloat*)&proj_mat);
        glUniformMatrix4fv(shader->uniforms[Shader::kModelviewMat4], 1, false, (GLfloat*)&view_mat);
        glUniformMatrix4fv(shader->uniforms[Shader::kBoneMatrices], 128, false, (GLfloat*)bone_transforms);
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

void GameState::Draw(GraphicsContext* context, int ticks, Profiler* profiler) {
    CHECK_GL_ERROR();

    glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
    glClearColor(0.5,0.5,0.5,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat = glm::perspective(camera_fov, aspect_ratio, 0.1f, 100.0f);
    mat4 view_mat = inverse(camera.GetMatrix());

    for(int i=0; i<kMaxCharacters; ++i){
        if(characters[i].exists) {
            drawables[characters[i].drawable].transform = characters[i].transform.GetCombination();
        }
    }

    profiler->StartEvent("Draw drawables");
    for(int i=0; i<num_drawables; ++i){
        Drawable* drawable = &drawables[i];
        CHECK_GL_ERROR();
        DrawDrawable(context, proj_mat, view_mat, drawable, profiler);
        CHECK_GL_ERROR();
    }
    profiler->EndEvent();

    static const bool draw_coordinate_grid = false;
    if(draw_coordinate_grid){
        CHECK_GL_ERROR();
        DrawCoordinateGrid(this);
        CHECK_GL_ERROR();
    }
    if(kDrawNavMesh){
        nav_mesh.Draw(context, proj_mat * view_mat);
        CHECK_GL_ERROR();
    }
    lines.Draw(context, proj_mat * view_mat);
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

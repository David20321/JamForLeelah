#include "crn_decomp.h"
#include "game/game_state.h"
#include "game/nav_mesh.h"
#include "game/assets.h"
#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/audio.h"
#include "platform_sdl/profiler.h"
#include "internal/common.h"
#include "internal/geometry.h"
#include "internal/memory.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/norm.hpp"
#include <SDL.h>
#include <GL/glew.h>
#include <cstring>

#ifndef WIN32
const int GameState::kMapSize;
#endif

using namespace glm;


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

void LoadOgg(OggTrack* ogg_track, const char* path, FileLoadThreadData* file_load_data, StackAllocator* stack_alloc) {
    StartLoadFile(path, file_load_data);
    ogg_track->mem_len = file_load_data->memory_len;
    ogg_track->mem = stack_alloc->Alloc(file_load_data->memory_len);
    if(!ogg_track->mem){
        FormattedError("Error", "Failed to allocate memory for ogg file: %s", path);
    }
    memcpy(ogg_track->mem, file_load_data->memory, ogg_track->mem_len);
    EndLoadFile(file_load_data);
#ifdef USE_STB_VORBIS
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
#else
    tOGVMemoryReader *memfile = new tOGVMemoryReader(ogg_track->mem, ogg_track->mem_len);
    int res = ov_open_callbacks(memfile, &ogg_track->vorbis, NULL, 0, OV_MEMORY_CALLBACKS);
    if (res < 0) {
        FormattedError("Error", "Could not open ogg: %s", path);
    }
    vorbis_info *info = ov_info(&ogg_track->vorbis, -1);
    SDL_assert(info->rate == 48000 && info->channels == 2);
    ogg_track->samples = (int)ov_pcm_total(&ogg_track->vorbis, -1);
    ogg_track->read_pos = 0;
    ogg_track->gain = 0.0f;
    ogg_track->target_gain = 0.0f;
    ogg_track->transition_speed = 0.0000003f;
    ogg_track->decoded = (float*)stack_alloc->Alloc(sizeof(float) * ogg_track->samples * info->channels);
    if(!ogg_track->decoded) {
        FormattedError("Error", "Failed to allocate memory for decoded ogg: %s", path);
    }
    int total_read = 0;

    while (1) {
        float **streams;
        int bitstream;

        int numread = ov_read_float(&ogg_track->vorbis, &streams, ogg_track->samples, &bitstream);
        if (numread < 0) {
            FormattedError("Error", "Failed to read ogg samples: %s", path);
            break;
        } else if (numread == 0) {
            break;
        }
        // Interleave the data
        for (int f = 0; f < numread; ++f) {
            for (int c = 0; c < info->channels; ++c) {
                ogg_track->decoded[total_read + f * info->channels + c] = streams[c][f];
            }
        }
        total_read += numread * info->channels;
    }
#endif
}

void LoadTTF(const char* path, TextAtlas* text_atlas, FileLoadThreadData* file_load_data, float pixel_height) {
    StartLoadFile(path, file_load_data);
    static const int kAtlasSize = 512;
    unsigned char temp_bitmap[kAtlasSize*kAtlasSize];
    stbtt_BakeFontBitmap((const unsigned char*)file_load_data->memory, 0, 
        pixel_height, temp_bitmap, 512, 512, 32, 96, text_atlas->cdata); // no guarantee this fits!
    EndLoadFile(file_load_data);
    GLuint tmp_texture;
    glGenTextures(1, &tmp_texture);
    text_atlas->texture = tmp_texture;
    text_atlas->pixel_height = pixel_height;
    glBindTexture(GL_TEXTURE_2D, text_atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kAtlasSize, kAtlasSize, 0,
        GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

void BoundingBoxFromParseMesh(ParseMesh* parse_mesh, vec3* bounding_box){
    bounding_box[0] = vec3(FLT_MAX);
    bounding_box[1] = vec3(-FLT_MAX);
    for(int i=0, vert_index=0; 
        i<parse_mesh->num_vert; 
        ++i, vert_index+=ParseMesh::kFloatsPerVert_Unskinned)
    {
        for(int k=0; k<3; ++k){
            bounding_box[1][k] = max(bounding_box[1][k], parse_mesh->vert[vert_index+k]);
            bounding_box[0][k] = min(bounding_box[0][k], parse_mesh->vert[vert_index+k]);
        }
    }
}

void LoadMeshAssetTxt(FileLoadThreadData* file_load_thread_data,
                      MeshAsset* mesh_asset, const char* path,
                      StackAllocator* stack_alloc) 
{
    ParseMesh parse_mesh;
    ParseTestFile(path, &parse_mesh, stack_alloc);
    mesh_asset->vert_vbo = 
        CreateVBO(kArrayVBO, kStaticVBO, parse_mesh.vert, 
        parse_mesh.num_vert*sizeof(float)*8);
    mesh_asset->index_vbo = 
        CreateVBO(kElementVBO, kStaticVBO, parse_mesh.indices, 
        parse_mesh.num_index*sizeof(Uint32));
    mesh_asset->num_index = parse_mesh.num_index;
    BoundingBoxFromParseMesh(&parse_mesh, mesh_asset->bounding_box);
    parse_mesh.Dispose();
}

void FillStaticDrawable(Drawable* drawable, const MeshAsset& mesh_asset, 
                        int texture, int shader, vec3 translation) 
{
    drawable->bounding_sphere_center = 
        (mesh_asset.bounding_box[0]+mesh_asset.bounding_box[1]) * 0.5f;
    drawable->bounding_sphere_radius = 
        length(mesh_asset.bounding_box[1] - mesh_asset.bounding_box[0])*0.5f;
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

void AddNavMeshAsset(NavMeshAsset* nav_mesh_asset, NavMesh* nav_mesh, const mat4& mat) {
    int start_verts = nav_mesh->num_verts;
    for(int i=0; i<nav_mesh_asset->num_verts; ++i){
        nav_mesh->verts[nav_mesh->num_verts++] = 
            vec3(mat * vec4(nav_mesh_asset->verts[i], 1));
    }
    for(int i=0; i<nav_mesh_asset->num_indices; ++i){
        nav_mesh->indices[nav_mesh->num_indices++] =
            start_verts + nav_mesh_asset->indices[i];
    }
}


int LoadCrnTexture(const char* path, FileLoadThreadData* file_load_data, 
                   StackAllocator* stack_alloc) 
{
    StartLoadFile(path, file_load_data);
    crnd::crn_texture_info tex_info;
    if (!crnd::crnd_get_texture_info(file_load_data->memory, 
                                     file_load_data->memory_len, &tex_info))
    {
        FormattedError("Error", "Error getting info of CRN file: %s", path);
        exit(1);
    }
    crnd::crnd_unpack_context context = crnd::crnd_unpack_begin(
        file_load_data->memory, 
        file_load_data->memory_len);
    if(!context){
        FormattedError("Error", "Error loading CRN file: %s", path);
        exit(1);
    }

    // Now transcode all face and mipmap levels into memory, one mip level at a time.
    void *pImages[cCRNMaxFaces][cCRNMaxLevels];
    crn_uint32 image_size_in_bytes[cCRNMaxLevels];
    memset(pImages, 0, sizeof(pImages));
    memset(image_size_in_bytes, 0, sizeof(image_size_in_bytes));
    
    GLuint tmp_texture;
    glGenTextures(1, &tmp_texture);
    int texture = tmp_texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // adapted from crnlib example2.cpp
    for (int level_index = 0, len=tex_info.m_levels; level_index < len; ++level_index) {
        const crn_uint32 width = max(1U, tex_info.m_width >> level_index);
        const crn_uint32 height = max(1U, tex_info.m_height >> level_index);
        const crn_uint32 blocks_x = max(1U, (width + 3) >> 2);
        const crn_uint32 blocks_y = max(1U, (height + 3) >> 2);
        const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(tex_info.m_format);
        const crn_uint32 total_face_size = row_pitch * blocks_y;

        for (crn_uint32 face_index = 0; face_index < tex_info.m_faces; ++face_index) {
            void *p = stack_alloc->Alloc(total_face_size);
            if(!p){
                FormattedError("Error", "Allocation failed at %s: %d", __FILE__, __LINE__);
                exit(1);
            }
            pImages[face_index][level_index] = p;
        }

        // Prepare the face pointer array needed by crnd_unpack_level().
        void *pDecomp_images[cCRNMaxFaces];
        for (crn_uint32 face_index = 0; face_index < tex_info.m_faces; face_index++)
            pDecomp_images[face_index] = pImages[face_index][level_index];

        // Now transcode the level to raw DXTn
        if (!crnd::crnd_unpack_level(context, pDecomp_images, total_face_size, row_pitch, level_index)) {
            FormattedError("Error", "Failed to unpack level of CRN texture");
            exit(1);
        }

        int internal_format = -1;
        switch(tex_info.m_format){
        case cCRNFmtDXT1:
            internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case cCRNFmtDXT5:
            internal_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        default:
            FormattedError("Error", "Unknown CRN texture format");
            exit(1);
        }

        glCompressedTexImage2D(GL_TEXTURE_2D, level_index, internal_format, width, height, 0, total_face_size, pDecomp_images[0]);
        CHECK_GL_ERROR();

        for (int face_index = tex_info.m_faces-1; face_index >= 0; --face_index)
        {
            stack_alloc->Free(pImages[face_index][level_index]);
        }
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, kMaxAnisotropy);

    if(!crnd::crnd_unpack_end(context)){
        FormattedError("Error", "Error closing CRN file context: %s", path);
        exit(1);
    }
    EndLoadFile(file_load_data);

    return texture;
}

void GameState::Init(int* init_stage, GraphicsContext* graphics_context, 
                     AudioContext* audio_context, Profiler* profiler, 
                     FileLoadThreadData* file_load_thread_data, 
                     StackAllocator* stack_allocator) 
{
    profiler->StartEvent("Game Init");
    profiler->StartEvent("Loading music");
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
    profiler->EndEvent();

    { // Allocate memory for debug lines
        int mem_needed = lines.AllocMemory(NULL);
        void* mem = stack_allocator->Alloc(mem_needed);
        if(!mem) {
            FormattedError("Error", "Could not allocate memory for DebugLines (%d bytes)", mem_needed);
        } else {
            lines.AllocMemory(mem);
        }
    }

    profiler->StartEvent("Loading static meshes");
    MeshAsset mesh_assets[kNumMesh];
    for(int i=0; i<kNumMesh; ++i){
        LoadMeshAssetTxt(file_load_thread_data, &mesh_assets[i], 
            asset_list[kStartStaticDrawMeshes+i+1], stack_allocator);
    }
    profiler->EndEvent();

    profiler->StartEvent("Loading nav meshes");
    NavMeshAsset nav_mesh_assets[kNumNavMesh];
    for(int i=0; i<kNumNavMesh; ++i){
        ParseMesh parse_mesh;
        ParseTestFile(asset_list[kStartNavMeshes+i+1], &parse_mesh, stack_allocator);
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
    profiler->EndEvent();

    profiler->StartEvent("Loading textures");
    int textures[kNumTex];
    for(int i=0; i<kNumTex; ++i){
        textures[i] = LoadCrnTexture(asset_list[kStartTextures+i+1], file_load_thread_data, 
                                     stack_allocator);
    }
    profiler->EndEvent();

    profiler->StartEvent("Loading shaders");
    int shaders[kNumShaders];
    for(int i=0; i<kNumShaders; ++i){
        shaders[i] = CreateProgramFromFile(graphics_context, file_load_thread_data, 
                                           asset_list[kStartShaders+i+1]);
    }
    profiler->EndEvent();
            
    lamp_shadow_tex = textures[TexID(kTexLampShadow)];
                  
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

    lines.vbo = CreateVBO(kArrayVBO, kStreamVBO, NULL, 
                          DebugDrawLines::kMaxLines * 
                          DebugDrawLines::kElementsPerPoint * 
                          2 * sizeof(GLfloat));

    profiler->StartEvent("Loading character assets");
    num_character_assets = 0;
    for(int i=0; i<kNumCharacterAssets; ++i){
        ParseMesh* parse_mesh = &character_assets[num_character_assets].parse_mesh;
        ParseTestFile(asset_list[kStartCharacterAssets+i+1], parse_mesh, stack_allocator);
        character_assets[num_character_assets].vert_vbo = 
            CreateVBO(kArrayVBO, kStaticVBO, parse_mesh->vert, 
            parse_mesh->num_vert*sizeof(float)*16);
        character_assets[num_character_assets].index_vbo = 
            CreateVBO(kElementVBO, kStaticVBO, parse_mesh->indices, 
            parse_mesh->num_index*sizeof(Uint32));
        BoundingBoxFromParseMesh(parse_mesh, character_assets[num_character_assets].bounding_box);
        ++num_character_assets;
    }
    profiler->EndEvent();

    for(int i=0; i<kMaxCharacters; ++i){
        characters[i].exists = false;
    }

    static const bool kOnlyOneCharacter = false;
    int num_chars = kOnlyOneCharacter?1:kMaxCharacters;

    profiler->StartEvent("Initializing characters");
    for(int i=0; i<num_chars; ++i){
        characters[i].rotation = 0.0f;
        characters[i].exists = true;
        characters[i].nav_mesh_walker.tri = -1;
        characters[i].tether_target = -1;
        if(i == 0){
            characters[i].character_asset = &character_assets[0];
            characters[i].transform.translation = vec3(kMapSize,0,kMapSize);
            characters[i].mind.state = Mind::kPlayerControlled;
            characters[i].type = Character::kPlayer;
            characters[i].revealed = true;
        } else {
            characters[i].character_asset = &character_assets[glm::linearRand(0,2)];
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
        if(characters[i].character_asset == &character_assets[0]){
            drawables[num_drawables].texture_id = textures[TexID(kTexChar)];
        } else if(characters[i].character_asset == &character_assets[1]){
            drawables[num_drawables].texture_id = 
                textures[TexID(glm::linearRand<int>(kTexWomanNPC1, kTexWomanNPC2))];
        } else if(characters[i].character_asset == &character_assets[2]){
            drawables[num_drawables].texture_id = 
                textures[TexID(glm::linearRand<int>(kTexManNPC1, kTexManNPC2))];
        }
        drawables[num_drawables].shader_id = shaders[ShaderID(kShader3DModelSkinned)];;
        drawables[num_drawables].character = &characters[i];
        drawables[num_drawables].bounding_sphere_center = 
            (characters[i].character_asset->bounding_box[0]+characters[i].character_asset->bounding_box[1]) * 0.5f;
        drawables[num_drawables].bounding_sphere_radius = 
            length(characters[i].character_asset->bounding_box[1] - characters[i].character_asset->bounding_box[0])*0.5f;
        ++num_drawables;
    }
    profiler->EndEvent();

    nav_mesh.num_verts = 0;
    nav_mesh.num_indices = 0;

    enum TileType {
        kNothing,
        kFloor,
        kWall,
        kCorner,
        kNook,
        kStairs,
        kLamp
    };
    TileType tiles[kMapSize * kMapSize];
    int rotation[kMapSize * kMapSize];

    for(int i=0, len=kMapSize*kMapSize; i<len; ++i){
        tiles[i] = kFloor;
        rotation[i] = 0;
        tile_height[i] = 0;//rand()%20==0;
    }


    tiles[13*kMapSize+13] = kLamp;
    tiles[15*kMapSize+15] = kNothing;
    tiles[15*kMapSize+16] = kStairs;
    tiles[16*kMapSize+15] = kNothing;
    tiles[16*kMapSize+16] = kNothing;
    tiles[15*kMapSize+19] = kNothing;
    tiles[15*kMapSize+20] = kStairs;
    tiles[16*kMapSize+19] = kNothing;
    tiles[16*kMapSize+20] = kNothing;
    rotation[15*kMapSize+20] = 2;
    tiles[17*kMapSize+17] = kNothing;
    tiles[17*kMapSize+18] = kStairs;
    tiles[18*kMapSize+17] = kNothing;
    tiles[18*kMapSize+18] = kNothing;
    rotation[17*kMapSize+18] = 3;
    tiles[13*kMapSize+17] = kNothing;
    tiles[13*kMapSize+18] = kStairs;
    tiles[14*kMapSize+17] = kNothing;
    tiles[14*kMapSize+18] = kNothing;
    rotation[13*kMapSize+18] = 1;
    tiles[17*kMapSize+16] = kCorner;
    tiles[14*kMapSize+16] = kCorner;
    rotation[14*kMapSize+16] = 1;
    tiles[14*kMapSize+19] = kCorner;
    rotation[14*kMapSize+19] = 2;
    tiles[17*kMapSize+19] = kCorner;
    rotation[17*kMapSize+19] = 3;

    tile_height[15*kMapSize+17] = 1;
    tile_height[16*kMapSize+17] = 1;
    tile_height[15*kMapSize+18] = 1;
    tile_height[16*kMapSize+18] = 1;

    static const bool kTilesFromTileHeight = false;
    if(kTilesFromTileHeight){
        for(int z=0; z<kMapSize; ++z){
            for(int x=0; x<kMapSize; ++x){
                int rotation;
                TileType tile_type;

                if(x<kMapSize-1 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)] && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                    tile_type = kNook;
                    rotation = 1;
                } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                    tile_type = kNook;
                    rotation = 3;
                } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)] && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                    tile_type = kNook;
                    rotation = 2;
                } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)] && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                    tile_type = kNook;
                    rotation = 0;
                } else if(x<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x+1)]){
                    tile_type = kWall;
                    rotation = 0;
                } else if(x>0 && tile_height[z*kMapSize+x] < tile_height[z*kMapSize+(x-1)]){
                    tile_type = kWall;
                    rotation = 2;
                } else if(z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+x]){
                    tile_type = kWall;
                    rotation = 3;
                } else if(z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+x]){
                    tile_type = kWall;
                    rotation = 1;
                } else if(x<kMapSize-1 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x+1)]){
                    tile_type = kCorner;
                    rotation = 1;
                } else if(x>0 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x-1)]){
                    tile_type = kCorner;
                    rotation = 3;
                } else if(x>0 && z<kMapSize-1 && tile_height[z*kMapSize+x] < tile_height[(z+1)*kMapSize+(x-1)]){
                    tile_type = kCorner;
                    rotation = 2;
                } else if(x<kMapSize-1 && z>0 && tile_height[z*kMapSize+x] < tile_height[(z-1)*kMapSize+(x+1)]){
                    tile_type = kCorner;
                    rotation = 0;
                } else {
                    tile_type = kFloor;
                    rotation = 0;
                }
            }
        }
    }

    profiler->StartEvent("Setting up tiles");
    for(int z=0; z<kMapSize; ++z){
        for(int x=0; x<kMapSize; ++x){
            int index = z*kMapSize + x;
            if(tiles[index] == kNothing){
                continue;
            }
            SeparableTransform transform;
            vec3 translation(x*2,tile_height[z*kMapSize+x]*2,z*2);
            switch(rotation[index]){
            case 0:
                break;
            case 1:
                transform.translation = vec3(0,0,2);
                transform.rotation = angleAxis(-half_pi<float>(), vec3(0,1,0));
                break;
            case 2:
                transform.translation = vec3(-2,0,2);
                transform.rotation = angleAxis(pi<float>(), vec3(0,1,0));
                break;
            case 3:
                transform.translation = vec3(-2,0,0);
                transform.rotation = angleAxis(half_pi<float>(), vec3(0,1,0));
                break;
            }
            if(tiles[index] == kStairs){
                transform.translation *= 2.0f;
            }
            transform.translation += translation;
            mat4 mat = transform.GetCombination();

            switch(tiles[index]){
            case kNook: {
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshGardenTallNook)], 
                    textures[TexID(kTexGardenTallNook)],
                    shaders[ShaderID(kShader3DModel)], vec3(0));
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavGardenTallNook)], &nav_mesh, mat);
                        } break;
            case kWall: {
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshGardenTallWall)], 
                    textures[TexID(kTexGardenTallWall)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavGardenTallWall)], &nav_mesh, mat);
                } break;
            case kCorner: {
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshGardenTallCorner)], 
                    textures[TexID(kTexGardenTallCorner)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavGardenTallCorner)], &nav_mesh, mat);
                } break;
            case kFloor: {
                int tex = glm::linearRand<int>(kTexFloor, kTexFloor4);
                if(rand()%20 == 0){
                    tex = kTexFloorManhole;
                }
                if(rand()%20 == 0){
                    tex = kTexFloorGrate;
                }
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshFloor)], 
                    textures[TexID(tex)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavFloor)], &nav_mesh, mat);
                } break;
            case kLamp: {
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshLamp)], 
                    textures[TexID(kTexLamp)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshFloor)], 
                    textures[TexID(kTexFloor)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavLamp)], &nav_mesh, mat);
                } break;
            case kStairs: {
                FillStaticDrawable(&drawables[num_drawables++], 
                    mesh_assets[MeshID(kMeshGardenTallStairs)], 
                    textures[TexID(kTexGardenTallStairs)],
                    shaders[ShaderID(kShader3DModel)], 
                    translation);
                AddNavMeshAsset(&nav_mesh_assets[NavMeshID(kNavGardenTallStairs)], &nav_mesh, mat);
                } break;
            }
            drawables[num_drawables-1].transform = mat;
        }
    }
    profiler->EndEvent();
    
    profiler->StartEvent("Creating nav mesh");
    nav_mesh.CalcNeighbors(stack_allocator);
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
    profiler->EndEvent();

    profiler->StartEvent("Placing characters in nav mesh");
    for(int i=0; i<num_chars; ++i){
        characters[i].nav_mesh_walker.tri = nav_mesh.ClosestTriToPoint(characters[i].transform.translation);
        vec3 tri_mid;
        for(int j=0; j<3; ++j){
            int tri_index = characters[i].nav_mesh_walker.tri * 3;
            tri_mid += nav_mesh.verts[nav_mesh.indices[tri_index]] / 3.0f;
        }
        characters[i].transform.translation = tri_mid;
    }
    profiler->EndEvent();
    profiler->EndEvent();

    *init_stage = -1;
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
            vec3 tri_normal = normalize(cross(tri_verts[2] - tri_verts[0], tri_verts[1] - tri_verts[0]));
            SDL_assert(tri_normal == tri_normal);
            float char_norm_d = dot(pos, tri_normal);
            float tri_norm_d = dot(tri_verts[0], tri_normal);
            SDL_assert(pos == pos);
            pos += tri_normal * (tri_norm_d - char_norm_d);
            SDL_assert(pos == pos);
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
            SDL_assert(pos == pos);
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

    character->color = vec4(1,1,1,1);
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
                    if(characters[characters[i].mind.seek_target].exists){
                        target_dir = characters[i].transform.translation - 
                            characters[characters[i].mind.seek_target].transform.translation;
                        if(characters[i].mind.state == Mind::kSeekTarget){
                            target_dir *= -1.0f;
                        }
                        float target_dir_len = length(target_dir);
                        if(target_dir_len > characters[i].mind.seek_target_distance[1] &&
                           target_dir_len > 0.001f)
                        {
                            target_dir = normalize(target_dir) * 0.5f;
                        } else if(target_dir_len < characters[i].mind.seek_target_distance[0] &&
                                  target_dir_len > 0.001f)
                        {
                            target_dir = normalize(target_dir) * -0.5f;                            
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
                num_lights = 2;
                light_pos[0] = characters[i].transform.translation + vec3(0,1,0);
                light_color[0] = vec3(5.0f,3.0f,0.0f);
                light_color[0] *= characters[i].energy;    
                light_type[0] = 0;
                light_pos[1] = vec3(13*2,2,13*2);
                light_pos[1]+= vec3(-0.8,0,1.0);
                light_color[1] = vec3(5.0f,3.0f,0.0f) * 10.0f;       
                light_color[1] *= sin(SDL_GetTicks()/1000.0f)*0.5f+0.5f;      
                light_type[1] = 1;        
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
                float turn_speed = 5.0f;
                if(character->type == Character::kPlayer){
                    turn_speed = 10.0f;
                }
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

void DrawDrawable(float *frustum_planes, GameState* game_state, 
                  GraphicsContext* graphics_context, const mat4 &proj_mat, 
                  const mat4 &view_mat, Drawable* drawable, Profiler* profiler) 
{
    mat4 modelview_mat = view_mat * drawable->transform;
    {
        vec3 test_pos(modelview_mat * vec4(drawable->bounding_sphere_center,1));
        for(int i=0; i<24; i+=4){
            float* plane = &frustum_planes[i];
            float val = dot(test_pos, normalize(vec3(plane[0], plane[1], plane[2])));
            if(val > plane[3] + drawable->bounding_sphere_radius){
                return;
            }
        }
    }
    Shader *shader = &graphics_context->shaders[drawable->shader_id];
    CHECK_GL_ERROR();
    glUseProgram(shader->gl_id);
    CHECK_GL_ERROR();

    mat3 normal_mat = mat3(drawable->transform);
    glUniformMatrix3fv(shader->uniforms[Shader::kNormalMat3], 1, false, (GLfloat*)&normal_mat);

    CHECK_GL_ERROR();
    glUniform1i(shader->uniforms[Shader::kNumLights], game_state->num_lights);
    glUniform3fv(shader->uniforms[Shader::kLightPos], game_state->num_lights, (GLfloat*)game_state->light_pos);
    glUniform3fv(shader->uniforms[Shader::kLightColor], game_state->num_lights, (GLfloat*)game_state->light_color);
    glUniform1iv(shader->uniforms[Shader::kLightType], game_state->num_lights, game_state->light_type);
    glUniform3fv(shader->uniforms[Shader::kFogColor], 1, (GLfloat*)&game_state->fog_color);
    glUniform1f(shader->uniforms[Shader::kTime], SDL_GetTicks()/1000.0f);
    glUniform1i(shader->uniforms[Shader::kLampTexID], 1);
    CHECK_GL_ERROR();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawable->texture_id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, game_state->lamp_shadow_tex);
    CHECK_GL_ERROR();

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
        glUniformMatrix4fv(shader->uniforms[Shader::kWorldMat4], 1, false, (GLfloat*)&drawable->transform);
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
        int frame = (int)character->walk_cycle_frame - parse_mesh->animations[animation].first_frame;
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

// Adapted from http://www.iquilezles.org/www/articles/frustum/frustum.htm
void iqFrustumF_CreatePerspective( float *frus, float fovy, float aspect, 
                                   float znear, float zfar )
{
    const float an = fovy * 0.5f;
    const float si = sinf(an);
    const float co = cosf(an);
    frus[ 0] =  0.0f; frus[ 1] = -co;      frus[ 2] =  si;        frus[ 3] =  0.0f;
    frus[ 4] =  0.0f; frus[ 5] =  co;      frus[ 6] =  si;        frus[ 7] =  0.0f;
    frus[ 8] =  co;   frus[ 9] =  0.0f;    frus[10] =  si*aspect; frus[11] =  0.0f;
    frus[12] = -co;   frus[13] =  0.0f;    frus[14] =  si*aspect; frus[15] =  0.0f;
    frus[16] =  0.0f; frus[17] =  0.0f;    frus[18] = -1.0f;      frus[19] =  zfar;
    frus[20] =  0.0f; frus[21] =  0.0f;    frus[22] =  1.0f;      frus[23] = -znear;
}

void SetProjectionMatrix(mat4 *proj_mat, float* planes, float fovy, float aspect, float near, float far){
    *proj_mat = glm::perspective(fovy, aspect, near, far);
    // Get frustum planes for culling
    iqFrustumF_CreatePerspective(planes, fovy, aspect, near, far);
    for(int i=0, index=0; i<6; ++i, index+=4){
        vec3 temp(planes[index+0], planes[index+1], planes[index+2]);
        temp = normalize(temp);
        planes[index+0] = temp[0];
        planes[index+1] = temp[1];
        planes[index+2] = temp[2];
    }
}

void GameState::Draw(GraphicsContext* context, int ticks, Profiler* profiler) {
    CHECK_GL_ERROR();

    fog_color = vec3(0.1,0.2,0.3);

    glViewport(0, 0, context->screen_dims[0], context->screen_dims[1]);
    glClearColor(fog_color[0],fog_color[1],fog_color[2],1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect_ratio = context->screen_dims[0] / (float)context->screen_dims[1];
    mat4 proj_mat;
    float planes[24];
    SetProjectionMatrix(&proj_mat, planes, camera_fov, aspect_ratio, 0.1f, 100.0f);
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
        DrawDrawable(planes, this, context, proj_mat, view_mat, drawable, profiler);
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
        profiler->StartEvent("Draw nav mesh");
        nav_mesh.Draw(context, proj_mat * view_mat);
        CHECK_GL_ERROR();
        profiler->EndEvent();
    }
    profiler->StartEvent("Draw debug lines");
    lines.Draw(context, profiler, proj_mat * view_mat);
    profiler->EndEvent();
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
                                other->mind.seek_target_distance[0] = 0.0f;
                                other->mind.seek_target_distance[1] = 0.0f;
                            } else if(other->type == Character::kGreen){
                                other->tether_target = char_ids[k];
                                other->mind.state = Mind::kSeekTarget;
                                other->mind.seek_target = char_ids[k];
                                other->mind.seek_target_distance[0] = 1.0f;
                                other->mind.seek_target_distance[1] = 2.0f;
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
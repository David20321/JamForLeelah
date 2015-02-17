#pragma once
#ifndef GAME_GAME_STATE_H
#define GAME_GAME_STATE_H

#include "glm/glm.hpp"
#include "game/nav_mesh.h"
#include "internal/separable_transform.h"
#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/debug_draw.h"
#include "platform_sdl/debug_text.h"

#ifdef WIN32
#define ASSET_PATH "../assets/"
#else
#define ASSET_PATH "assets/"
#endif

class FileLoadThreadData;
struct GraphicsContext;
class ParseMesh;
class Profiler;

struct CharacterAsset {
    static const int kMaxBones = 128;
    ParseMesh parse_mesh;
    glm::mat4 bind_transforms[128];
    int vert_vbo;
    int index_vbo;
};

struct Character {
    glm::vec3 velocity;
    SeparableTransform transform;
    NavMeshWalker nav_mesh_walker;
    static const int kWalkCycleStart = 31;
    static const int kWalkCycleEnd = 58;
    float walk_cycle_frame;
    CharacterAsset* character_asset;
};

struct Camera {
    float rotation_x;
    float rotation_y;
    glm::vec3 position;
    glm::quat GetRotation();
    glm::mat4 GetMatrix();
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
    Character* character;
    VBO_Setup vbo_layout;
    glm::mat4 transform;
};

class GameState {
public:
    static const int kMaxDrawables = 1000;
    static const int kMaxCharacters = 100;
    static const int kMaxCharacterAssets = 4;
    int num_character_assets;
    CharacterAsset character_assets[kMaxCharacterAssets];
    Drawable drawables[kMaxDrawables];
    int num_drawables;
    DebugDrawLines lines;
    DebugText debug_text;
    float camera_fov;
    int num_characters;
    Character characters[kMaxCharacters];
    Camera camera;
    int char_drawable;
    bool editor_mode;
    TextAtlas text_atlas;
    NavMesh nav_mesh;

    static const int kMapSize = 30;
    int tile_height[kMapSize * kMapSize];

    void Update(const glm::vec2& mouse_rel, float time_step);
    void Init(Profiler* profiler, FileLoadThreadData* file_load_thread_data, StackAllocator* stack_allocator);
    void Draw(GraphicsContext* context, int ticks);
};

#endif
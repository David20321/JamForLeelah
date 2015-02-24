#pragma once
#ifndef GAME_GAME_STATE_H
#define GAME_GAME_STATE_H

#include "glm/glm.hpp"
#include "game/nav_mesh.h"
#include "internal/separable_transform.h"
#include "platform_sdl/blender_file_io.h"
#include "platform_sdl/debug_draw.h"
#include "platform_sdl/debug_text.h"
#include "platform_sdl/audio.h"

#ifdef WIN32
#define ASSET_PATH "../assets/"
#else
#define ASSET_PATH "assets/"
#endif

class FileLoadThreadData;
struct GraphicsContext;
struct AudioContext;
class ParseMesh;
class Profiler;

struct CharacterAsset {
    ParseMesh parse_mesh;
    int vert_vbo;
    int index_vbo;
    glm::vec3 bounding_box[2];
};

struct Mind {
    enum State {
        kPlayerControlled,
        kWander,
        kSeekTarget,
        kStand
    };
    int wander_update_time;
    int seek_target;
    float seek_target_distance[2];
    State state;
    glm::vec3 dir;
};

struct Character {
    bool exists;
    enum Type {
        kPlayer,
        kGreen,
        kRed
    };
    int drawable;
    glm::vec3 velocity;
    SeparableTransform transform;
    NavMeshWalker nav_mesh_walker;
    static const int kWalkCycleStart = 31;
    static const int kWalkCycleEnd = 58;
    float walk_cycle_frame;
    CharacterAsset* character_asset;
    float rotation;
    Mind mind;
    glm::vec4 color;
    bool revealed;
    int tether_target;
    Type type;
    float energy;
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
    glm::vec3 bounding_sphere_center;
    float bounding_sphere_radius;
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
    Character characters[kMaxCharacters];
    Camera camera;
    int char_drawable;
    bool editor_mode;
    TextAtlas text_atlas;
    NavMesh nav_mesh;
    static const int kMaxOggTracks = 10;
    int num_ogg_tracks;
    OggTrack ogg_track[kMaxOggTracks];
    static const int kMaxLights = 10;
    int num_lights;
    glm::vec3 light_pos[kMaxLights];
    glm::vec3 light_color[kMaxLights];
    int light_type[kMaxLights];
    glm::vec3 fog_color;
    int lamp_shadow_tex;

    static const int kMapSize = 30;
    int tile_height[kMapSize * kMapSize];

    void Update(const glm::vec2& mouse_rel, float time_step);
    void Init(int* init_stage, GraphicsContext* graphics_context, AudioContext* audio_context, 
              Profiler* profiler, FileLoadThreadData* file_load_thread_data, 
              StackAllocator* stack_allocator);
    void Draw(GraphicsContext* context, int ticks, Profiler* profiler);
    void CharacterCollisions(Character* characters, float time_step);
};

#endif
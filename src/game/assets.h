#pragma once
#ifndef GAME_ASSETS_H
#define GAME_ASSETS_H

// To add an asset make sure to add the path to asset_list
// and a matching enum below
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
    ASSET_PATH "art/garden_tall_corner_nav_export.txt",
    ASSET_PATH "art/garden_tall_nook_nav_export.txt",
    ASSET_PATH "art/garden_tall_stairs_nav_export.txt",
    ASSET_PATH "art/floor_quad_export.txt",
    ASSET_PATH "art/street_lamp_nav_export.txt",
    "end_nav_meshes",
    "start_character_assets",
    ASSET_PATH "art/main_character_rig_export.txt",
    ASSET_PATH "art/woman_npc_rig_export.txt",
    ASSET_PATH "art/man_npc_rig_export.txt",
    "end_character_assets",
    "start_textures",
    ASSET_PATH "art/lamp_c.png",
    ASSET_PATH "art/dry_fountain_c.png",
    ASSET_PATH "art/flowerbox_c.png",
    ASSET_PATH "art/garden_tall_corner_c.png",
    ASSET_PATH "art/garden_tall_nook_c.png",
    ASSET_PATH "art/garden_tall_stairs.png",
    ASSET_PATH "art/garden_tall_wall_c.png",
    ASSET_PATH "art/short_wall_c.png",
    ASSET_PATH "art/tree_c.png",
    ASSET_PATH "art/wall_pillar_c.png",
    ASSET_PATH "art/tiling_cobbles_c.png",
    ASSET_PATH "art/tiling_cobbles_2_c.png",
    ASSET_PATH "art/tiling_cobbles_3_c.png",
    ASSET_PATH "art/tiling_stain.png",
    ASSET_PATH "art/tiling_manhole.png",
    ASSET_PATH "art/tiling_grate.png",
    ASSET_PATH "art/main_character_c.png",
    ASSET_PATH "art/woman_npc_c.png",
    ASSET_PATH "art/woman_npc_2_c.png",
    ASSET_PATH "art/man_npc_c.png",
    ASSET_PATH "art/man_npc_2.png",
    ASSET_PATH "art/lampshadow.png",
    "end_textures",
    "start_fonts",
    ASSET_PATH "fonts/LiberationMono-Regular.ttf",
    "end_fonts",
    "start_shaders",
#ifdef USE_OPENGLES
    ASSET_PATH "shaders_gles/3D_model",
    ASSET_PATH "shaders_gles/3D_model_skinned",
    ASSET_PATH "shaders_gles/debug_draw",
    ASSET_PATH "shaders_gles/debug_draw_text",
    ASSET_PATH "shaders_gles/nav_mesh",
#else
    ASSET_PATH "shaders/3D_model",
    ASSET_PATH "shaders/3D_model_skinned",
    ASSET_PATH "shaders/debug_draw",
    ASSET_PATH "shaders/debug_draw_text",
    ASSET_PATH "shaders/nav_mesh",
#endif
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
    kNavGardenTallCorner,
    kNavGardenTallNook,
    kNavGardenTallStairs,
    kNavFloor,
    kNavLamp,
    kEndNavMeshes,

    kStartCharacterAssets,
    kModelChar,
    kModelWomanNPC,
    kModelManNPC,
    kEndCharacterAssets,

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
    kTexFloor2,
    kTexFloor3,
    kTexFloor4,
    kTexFloorGrate,
    kTexFloorManhole,
    kTexChar,
    kTexWomanNPC1,
    kTexWomanNPC2,
    kTexManNPC1,
    kTexManNPC2,
    kTexLampShadow,
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

// Utility functions to access assets of a specific type

static const int kNumMesh = kEndStaticDrawMeshes-kStartStaticDrawMeshes-1;
inline int MeshID(int id){
    return id - kStartStaticDrawMeshes - 1;
}

static const int kNumTex = kEndTextures-kStartTextures-1;
inline int TexID(int id){
    return id - kStartTextures - 1;
}

static const int kNumShaders = kEndShaders-kStartShaders-1;
inline int ShaderID(int id){
    return id - kStartShaders - 1;
}

static const int kNumNavMesh = kEndNavMeshes-kStartNavMeshes-1;
inline int NavMeshID(int id){
    return id - kStartNavMeshes - 1;
}

static const int kNumCharacterAssets = kEndCharacterAssets-kStartCharacterAssets-1;
inline int CharacterAssetID(int id){
    return id - kStartCharacterAssets - 1;
}

#endif
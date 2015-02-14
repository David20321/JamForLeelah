#pragma once
#ifndef PLATFORM_SDL_DEBUG_TEXT_HPP
#define PLATFORM_SDL_DEBUG_TEXT_HPP

#include "stb_truetype.h"
#include <cstdio>

struct GraphicsContext;

struct TextAtlas {
    stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
    int texture;
    int shader;
    int vert_vbo;
    int index_vbo;
    float pixel_height;
};

struct DebugTextEntry {
    bool display;
    static const int kDebugTextStrMaxLen = 512;
    float fade_time;
    char str[kDebugTextStrMaxLen];
};

class DebugText {
public:
    static const int kMaxDebugTextEntries = 100;
    DebugTextEntry entries[kMaxDebugTextEntries];
    int free_queue[kMaxDebugTextEntries];
    int free_queue_start;
    int free_queue_end;
    TextAtlas *text_atlas;

    void Init(TextAtlas* p_text_atlas);
    int GetDebugTextHandle();
    void UpdateDebugText(int handle, float fade_time, const char* fmt, ...);
    void UpdateDebugTextV(int handle, float fade_time, const char* fmt, va_list args);
    void ReleaseDebugTextHandle(int handle);
    void Draw(GraphicsContext* context);
};

void DrawText(TextAtlas *text_atlas, GraphicsContext* context, float x, float y, char *text);

#endif
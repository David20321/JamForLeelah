#include "platform_sdl/debug_text.h"
#include "platform_sdl/graphics.h"
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"
#include "GL/glew.h"
#include "GL/GL.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "internal/common.h"

void DrawText(TextAtlas *text_atlas, GraphicsContext* context, float x, float y, char *text) {
    CHECK_GL_ERROR();
    static const int kMaxDrawStringLength = 1024;
    GLfloat vert_data[kMaxDrawStringLength*16]; // Four verts per character, 2V 2T per vert
    GLuint index_data[kMaxDrawStringLength*6]; // Two tris per character
    int num_draw_chars = 0;
    int vert_index=0, index_index=0;
    for(char* text_iter = text; *text_iter != '\0'; ++text_iter) {
        if (*text_iter >= 32 && *text_iter < 128 && num_draw_chars < kMaxDrawStringLength) {
            int vert_ref = num_draw_chars*4;
            ++num_draw_chars;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(text_atlas->cdata, 512, 512, *text_iter-32, &x, &y, &q, 1);
            vert_data[vert_index++] = q.x0;
            vert_data[vert_index++] = q.y0;
            vert_data[vert_index++] = q.s0;
            vert_data[vert_index++] = q.t0;

            vert_data[vert_index++] = q.x1;
            vert_data[vert_index++] = q.y0;
            vert_data[vert_index++] = q.s1;
            vert_data[vert_index++] = q.t0;

            vert_data[vert_index++] = q.x1;
            vert_data[vert_index++] = q.y1;
            vert_data[vert_index++] = q.s1;
            vert_data[vert_index++] = q.t1;

            vert_data[vert_index++] = q.x0;
            vert_data[vert_index++] = q.y1;
            vert_data[vert_index++] = q.s0;
            vert_data[vert_index++] = q.t1;

            index_data[index_index++] = vert_ref + 0;
            index_data[index_index++] = vert_ref + 1;
            index_data[index_index++] = vert_ref + 2;

            index_data[index_index++] = vert_ref + 0;
            index_data[index_index++] = vert_ref + 2;
            index_data[index_index++] = vert_ref + 3;
        }
    }
    CHECK_GL_ERROR();

    Shader* shader = &context->shaders[text_atlas->shader];

    glm::mat4 proj_mat = glm::ortho(0.0f, (float)context->screen_dims[0], 
        (float)context->screen_dims[1], 0.0f, -1.0f, 1.0f);
    CHECK_GL_ERROR();
    glUseProgram(shader->gl_id);
    CHECK_GL_ERROR();
    glUniformMatrix4fv(shader->uniforms[Shader::kProjectionMat4], 1, false, (GLfloat*)&proj_mat);
    CHECK_GL_ERROR();
    glUniform1i(shader->uniforms[Shader::kTextureID], 0);
    CHECK_GL_ERROR();
    glActiveTexture(GL_TEXTURE0);
    CHECK_GL_ERROR();
    glBindTexture(GL_TEXTURE_2D, text_atlas->texture);
    CHECK_GL_ERROR();

    glBindBuffer(GL_ARRAY_BUFFER, text_atlas->vert_vbo);
    glBufferData(GL_ARRAY_BUFFER, num_draw_chars*sizeof(GLfloat)*16, vert_data, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_atlas->index_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_draw_chars*sizeof(GLuint)*6, index_data, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2*sizeof(GLfloat)));
    glDrawElements(GL_TRIANGLES, num_draw_chars*6, GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    CHECK_GL_ERROR();
}

void DebugText::Draw(GraphicsContext* context, float time) {
    int num_draw = 0;
    for(int i=0; i<kMaxDebugTextEntries; ++i){
        DebugTextEntry& entry = entries[i];
        if(entry.display && time < entry.fade_time){
            DrawText(text_atlas, context, 40.0f, 40.0f + num_draw * text_atlas->pixel_height * 1.15f, entry.str);
            ++num_draw;
        }
    }
}

void DebugText::Init(TextAtlas* p_text_atlas) {
    text_atlas = p_text_atlas;
    for(int i=0; i<kMaxDebugTextEntries; ++i){
        free_queue[i] = i;
    }
    for(int i=0; i<kMaxDebugTextEntries; ++i){
        entries[i].display = false;
    }
    free_queue_start = 0;
    free_queue_end = kMaxDebugTextEntries-1;
}

int DebugText::GetDebugTextHandle() {
    if(free_queue_start != free_queue_end){
        int ret = free_queue[free_queue_start];
        entries[ret].display = true;
        free_queue_start = (free_queue_start+1)%kMaxDebugTextEntries;
        return ret;
    } else {
        SDL_assert(false);
        return -1;
    }
}

void DebugText::ReleaseDebugTextHandle(int handle) {
    entries[handle].display = false;
    free_queue_end = (free_queue_end+1)%kMaxDebugTextEntries;
    free_queue[free_queue_end] = handle;
}

void DebugText::UpdateDebugTextV(int handle, float fade_time, const char* fmt, va_list args) {
    SDL_assert(handle >= 0 && handle < kMaxDebugTextEntries && entries[handle].display);
    VFormatString(entries[handle].str, DebugTextEntry::kDebugTextStrMaxLen, fmt, args);
    entries[handle].fade_time = fade_time;
}


void DebugText::UpdateDebugText(int handle, float fade_time, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    UpdateDebugTextV(handle, fade_time, fmt, args);
    va_end(args);
}

#include "platform_sdl/debug_draw.h"
#include "glm/glm.hpp"
#include "GL/glew.h"
#include <cstring>
#include "platform_sdl/graphics.h"

using namespace glm;

bool DebugDrawLines::Add(const vec3& start, const vec3& end, 
                         const vec4& color, DebugDrawLifetime lifetime, 
                         int lifetime_int) 
{
    if(num_lines < kMaxLines - 1){
        DebugDrawCommon& new_line = common[num_lines];
        int line_index = num_lines * kElementsPerPoint * 2;
        for(int k=0; k<3; ++k){
            draw_data[line_index++] = start[k];
        }
        for(int k=0; k<4; ++k){
            draw_data[line_index++] = color[k];
        }
        for(int k=0; k<3; ++k){
            draw_data[line_index++] = end[k];
        }
        for(int k=0; k<4; ++k){
            draw_data[line_index++] = color[k];
        }
        ++num_lines;
        new_line.lifetime = lifetime;
        new_line.lifetime_int = lifetime_int;
        return true;
    } else {
        return false;
    }
}

void DebugDrawLines::Draw(const glm::mat4& proj_view_mat) {
    for(int i=0; i<num_lines;){
        DebugDrawCommon& line = common[i];
        if(line.lifetime_int <= 0){
            line = common[num_lines-1];
            memcpy(&draw_data[i*14], &draw_data[(num_lines-1)*14], 
                sizeof(GLfloat)*14);
            --num_lines;
        } else {
            ++i;
        }
    }

    CHECK_GL_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    CHECK_GL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, num_lines*sizeof(GLfloat)*kElementsPerPoint*2, draw_data, GL_STREAM_DRAW);
    CHECK_GL_ERROR();
    glUseProgram(shader);
    CHECK_GL_ERROR();
    GLuint modelview_matrix_uniform = glGetUniformLocation(shader, "mv_mat");
    CHECK_GL_ERROR();
    glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&proj_view_mat);
    CHECK_GL_ERROR();
    glEnableVertexAttribArray(0);
    CHECK_GL_ERROR();
    glEnableVertexAttribArray(1);
    CHECK_GL_ERROR();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), 0);
    CHECK_GL_ERROR();
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
    CHECK_GL_ERROR();
    glDrawArrays(GL_LINES, 0, num_lines*2);
    CHECK_GL_ERROR();
    glDisableVertexAttribArray(1);
    CHECK_GL_ERROR();
    glDisableVertexAttribArray(0);
    CHECK_GL_ERROR();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    CHECK_GL_ERROR();
    glUseProgram(0);
    CHECK_GL_ERROR();

    for(int i=0; i<num_lines; ++i){        
        DebugDrawCommon& line = common[i];
        if(line.lifetime == kDraw){
            --line.lifetime_int;
        }
    }
}

void DebugDrawLines::Update() {
    for(int i=0; i<num_lines; ++i){        
        DebugDrawCommon& line = common[i];
        if(line.lifetime == kUpdate){
            --line.lifetime_int;
        }
    }
}

int DebugDrawLines::AllocMemory(void* mem) {
    int common_size = kMaxLines * sizeof(DebugDrawCommon);
    int draw_data_size = kElementsPerPoint * kMaxLines * 2 * sizeof(float);
    if(mem){
        common = (DebugDrawCommon*)mem;
        draw_data = (float*)(((intptr_t)mem) + common_size);
    }
    return common_size + draw_data_size;
}

static const int s_pos_x = 1 << 0, s_pos_y = 1 << 1, s_pos_z = 1 << 2;
static const int e_pos_x = 1 << 3, e_pos_y = 1 << 4, e_pos_z = 1 << 5;

static void AddBBLine(DebugDrawLines* lines, const glm::mat4& mat, glm::vec3 bb[], int flags) {
    glm::vec3 points[2];
    points[0][0] = (flags & s_pos_x)?bb[1][0]:bb[0][0];
    points[0][1] = (flags & s_pos_y)?bb[1][1]:bb[0][1];
    points[0][2] = (flags & s_pos_z)?bb[1][2]:bb[0][2];
    points[1][0] = (flags & e_pos_x)?bb[1][0]:bb[0][0];
    points[1][1] = (flags & e_pos_y)?bb[1][1]:bb[0][1];
    points[1][2] = (flags & e_pos_z)?bb[1][2]:bb[0][2];
    lines->Add(glm::vec3(mat*glm::vec4(points[0],1.0f)), 
        glm::vec3(mat*glm::vec4(points[1],1.0f)), 
        glm::vec4(1.0f), kPersistent, 1);
}

void DrawBoundingBox(DebugDrawLines* lines, const glm::mat4& mat, glm::vec3 bb[]) {
    static const int s_neg_x = 0, s_neg_y = 0, s_neg_z = 0;
    static const int e_neg_x = 0, e_neg_y = 0, e_neg_z = 0;
    // Neg Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_neg_y | e_neg_z);
    // Pos Y square
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_neg_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_pos_y | s_pos_z | e_neg_x | e_pos_y | e_neg_z);
    // Neg Y to Pos Y
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_neg_z | e_neg_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_neg_z | e_pos_x | e_pos_y | e_neg_z);
    AddBBLine(lines, mat, bb, s_pos_x | s_neg_y | s_pos_z | e_pos_x | e_pos_y | e_pos_z);
    AddBBLine(lines, mat, bb, s_neg_x | s_neg_y | s_pos_z | e_neg_x | e_pos_y | e_pos_z);
}
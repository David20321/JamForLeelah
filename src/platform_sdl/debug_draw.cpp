#include "platform_sdl/debug_draw.h"
#include "glm/glm.hpp"
#include "GL/glew.h"

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
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, num_lines*sizeof(GLfloat)*kElementsPerPoint*2, draw_data, GL_STREAM_DRAW);
	glUseProgram(shader);
	GLuint modelview_matrix_uniform = glGetUniformLocation(shader, "mv_mat");
	glUniformMatrix4fv(modelview_matrix_uniform, 1, false, (GLfloat*)&proj_view_mat);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
	glDrawArrays(GL_LINES, 0, num_lines*2);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

	for(int i=0; i<num_lines; ++i){		
		DebugDrawCommon& line = common[i];
		if(line.lifetime == kDraw){
			--line.lifetime_int;
		}
	}

	for(int i=0; i<num_lines;){
		DebugDrawCommon& line = common[i];
		if(line.lifetime == kDraw && line.lifetime_int <= 0){
			line = common[num_lines-1];
			memcpy(&draw_data[i*14], &draw_data[(num_lines-1)*14], 
				   sizeof(GLfloat)*14);
			--num_lines;
		} else {
			++i;
		}
	}
}

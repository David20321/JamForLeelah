#version 330 

uniform mat4 proj_mat; 
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
out vec2 var_uv;

void main() { 
	gl_Position = proj_mat * vec4(position, 0.0, 1.0);
	var_uv = uv;
}
#version 330 

uniform mat4 mv_mat; 
layout(location = 0) in vec3 position;

void main() { 
	gl_Position = mv_mat * vec4(position + vec3(0.0, 0.1, 0.0), 1.0);
}
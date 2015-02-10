#version 330 

uniform mat4 mv_mat; 
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
out vec4 var_color;

void main() { 
	gl_Position = mv_mat * vec4(position, 1.0);
	var_color = color;
}
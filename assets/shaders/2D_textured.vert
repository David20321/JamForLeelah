#version 330 

uniform mat4 mv_mat; 
layout(location = 0) in vec4 position; 
out vec2 var_uv; 

void main() { 
	gl_Position = mv_mat * vec4(position.xy, 0.0, 1.0); 
	var_uv = position.zw; 
}
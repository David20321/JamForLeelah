#version 330 

uniform sampler2D texture_id;
in vec2 var_uv; 
out vec4 outputColor; 

void main() { 
   outputColor = texture(texture_id, var_uv); 
}
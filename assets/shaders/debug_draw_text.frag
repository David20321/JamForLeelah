#version 330 

uniform sampler2D texture_id;

in vec2 var_uv;
out vec4 outputColor; 

void main() { 
   outputColor = vec4(vec3(1.0), texture(texture_id, var_uv).r); 
}
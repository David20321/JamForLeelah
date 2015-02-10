#version 330 

uniform sampler2D texture_id;
in vec2 var_uv; 
in vec3 var_normal; 
out vec4 outputColor; 

void main() { 
   outputColor = texture(texture_id, var_uv); 
   //outputColor.xyz = var_normal * 0.5 + vec3(0.5); 
}
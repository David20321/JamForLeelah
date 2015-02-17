#version 330 

uniform sampler2D texture_id;
in vec3 var_view_pos; 
in vec2 var_uv; 
in vec3 var_normal; 
out vec4 outputColor; 

vec4 ApplyFog(vec4 color) {
    vec3 fog_color = vec3(0.5,0.5,0.5);
	float depth = length(var_view_pos);
	return vec4(mix(color.xyz, fog_color, max(0.0, min(1.0, (depth - 10.0) * 0.1))), color.a);
}

vec4 ApplyLighting(vec4 color, vec3 normal) {
	return vec4(color.xyz * vec3(mix((normal.y+1.0)*0.5, 1.0, 0.5)), color.a);
}

void main() { 
   outputColor = texture(texture_id, var_uv);
   outputColor = ApplyLighting(outputColor, var_normal); 
   outputColor = ApplyFog(outputColor); 
   //outputColor.xyz = var_normal * 0.5 + vec3(0.5); 
}
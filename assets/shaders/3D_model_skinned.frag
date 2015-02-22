#version 330 

uniform sampler2D texture_id;
uniform vec4 color;
uniform float energy;
uniform int num_lights;
uniform vec3 lights[8];
in vec3 var_view_pos; 
in vec2 var_uv; 
in vec3 var_normal; 
in vec3 var_world_pos; 
out vec4 outputColor; 

vec4 ApplyFog(vec4 color) {
    vec3 fog_color = vec3(0.1,0.1,0.1);
	float depth = length(var_view_pos);
	return vec4(mix(color.xyz, fog_color, max(0.0, min(1.0, (depth - 10.0) * 0.1))), color.a);
}

vec4 ApplyLighting(vec4 color) {
    vec3 light_amount = vec3(0.0);
    for(int i=0; i<num_lights; ++i){
    	float light_falloff = max(0.0, 1.0 / ( (distance(var_world_pos, lights[i]) * distance(var_world_pos, lights[i]) + 0.001) ) );
    	light_amount += light_falloff * /*max(0.0, dot(lights[i] - var_world_pos, var_normal)) * */vec3(0.5,0.5,1.0);
    }
    return color * vec4(light_amount, 1.0);
}

void main() { 
   outputColor = texture(texture_id, var_uv);
   outputColor *= color;
   outputColor = ApplyLighting(outputColor); 
   outputColor = ApplyFog(outputColor); 
   //outputColor.xyz = var_normal * 0.5 + vec3(0.5); 
}
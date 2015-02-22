#version 330 

uniform sampler2D texture_id; 
uniform sampler2D lamp_tex_id; 
uniform int num_lights;
uniform vec3 light_pos[8];
uniform vec3 light_color[8];
uniform int light_type[8];
uniform vec3 fog_color;
uniform float time;
in vec3 var_view_pos; 
in vec2 var_uv; 
in vec3 var_normal; 
in vec3 var_world_pos; 
out vec4 outputColor;

vec4 ApplyFog(vec4 color) {
	float depth = length(var_view_pos);
	vec3 fog_sample_pos = var_world_pos + vec3(time * 0.5f + sin(var_world_pos.z + var_world_pos.y)*0.2, 
											   time * 0.4f + sin(var_world_pos.x + var_world_pos.y)*0.3, 
											   time * 1.6f + sin(var_world_pos.z + var_world_pos.y)*0.4);
	depth += sin(fog_sample_pos.x - fog_sample_pos.y) * 0.75;
	depth += sin(fog_sample_pos.z * 1.5 - fog_sample_pos.y * 2.0) * 0.75;
	return vec4(mix(color.xyz, fog_color, max(0.0, min(1.0, (depth - 10.0) * 0.07))), color.a);
}

vec4 ApplyLighting(vec4 color) {
    vec3 light_amount = vec3(0.0);
    for(int i=0; i<num_lights; ++i){
    	float light_falloff = max(0.0, 1.0 / ( (distance(var_world_pos, light_pos[i]) * distance(var_world_pos, light_pos[i]) + 0.001) ) );
    	if(light_type[i] == 1){
	    	vec2 lamp_uv;
	    	lamp_uv[0] = var_world_pos.x - light_pos[i].x;
	    	lamp_uv[1] = var_world_pos.z - light_pos[i].z;
	    	lamp_uv *= vec2(0.05);
	    	lamp_uv += vec2(0.5);
	    	lamp_uv[0] += 0.05;
	    	lamp_uv[0] = clamp(lamp_uv[0], 0.0, 1.0);
	    	lamp_uv[1] = clamp(lamp_uv[1], 0.0, 1.0);
	    	light_falloff *= texture(lamp_tex_id, lamp_uv).r;
    	}
    	light_falloff *= max(0.0, dot(var_normal, normalize(light_pos[i] - var_world_pos)));
    	light_amount += light_falloff * light_color[i];
    }
    return color * vec4(light_amount, 1.0);
}


void main() { 
    outputColor = texture(texture_id, var_uv); 
    outputColor = ApplyLighting(outputColor);
    outputColor = ApplyFog(outputColor);
    //outputColor.xyz = var_normal * 0.5 + vec3(0.5); 
}
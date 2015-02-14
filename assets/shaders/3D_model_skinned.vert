#version 330 

uniform mat4 mv_mat; 
uniform mat4 bone_matrices[128];
uniform mat3 norm_mat; 
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv; 
layout(location = 2) in vec3 normal; 
layout(location = 3) in vec4 indices; 
layout(location = 4) in vec4 weights; 
out vec2 var_uv; 
out vec3 var_normal; 

void main() { 
	mat4 skinned_mat = mat4(0.0);
	for(int i=0; i<4; ++i){
		int index = int(indices[i]+0.5);
		if(indices[i] != -1.0){
			skinned_mat += bone_matrices[index] * weights[i];
		}
	}
	gl_Position = mv_mat * skinned_mat * vec4(position, 1.0);
	var_uv = uv;
	var_uv.y *= -1.0;
	var_normal = normalize(mat3(skinned_mat) * normal);
}
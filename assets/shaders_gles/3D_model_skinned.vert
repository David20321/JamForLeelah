uniform mat4 mv_mat; 
uniform mat4 proj_mat; 
uniform mat4 bone_matrices[128];
uniform mat3 norm_mat; 
attribute vec3 position;
attribute vec2 uv;
attribute vec3 normal;
attribute vec4 indices;
attribute vec4 weights;
varying vec2 var_uv;
varying vec3 var_normal;
varying vec3 var_view_pos;

void main() { 
	mat4 skinned_mat = mat4(0.0);
	for(int i=0; i<4; ++i){
		int index = int(indices[i]+0.5);
		if(indices[i] != -1.0){
			skinned_mat += bone_matrices[index] * weights[i];
		}
	}
	gl_Position = proj_mat * mv_mat * skinned_mat * vec4(position, 1.0);
	var_uv = uv;
	var_uv.y *= -1.0;
	var_normal = normalize(mat3(skinned_mat) * normal);
	var_view_pos = vec3(mv_mat * skinned_mat * vec4(position, 1.0));
}
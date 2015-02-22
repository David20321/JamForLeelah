uniform mat4 mv_mat; 
uniform mat4 proj_mat; 
uniform mat3 norm_mat; 
attribute vec3 position;
attribute vec2 uv;
attribute vec3 normal;
varying vec2 var_uv;
varying vec3 var_normal;
varying vec3 var_view_pos;

void main() { 
	gl_Position = proj_mat * mv_mat * vec4(position, 1.0);
	var_view_pos = vec3(mv_mat * vec4(position, 1.0));
	var_uv = uv;
	var_uv.y *= -1.0;
	var_normal = normal;
}
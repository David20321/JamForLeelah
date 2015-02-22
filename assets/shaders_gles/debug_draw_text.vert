uniform mat4 proj_mat;
attribute vec2 position;
attribute vec2 uv;
varying vec2 var_uv;

void main() { 
	gl_Position = proj_mat * vec4(position, 0.0, 1.0);
	var_uv = uv;
}
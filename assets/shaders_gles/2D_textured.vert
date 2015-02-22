uniform mat4 mv_mat;
attribute vec4 position;

varying vec2 var_uv;

void main() { 
	gl_Position = mv_mat * vec4(position.xy, 0.0, 1.0); 
	var_uv = position.zw; 
}
uniform mat4 mv_mat; 
attribute vec3 position;
attribute vec4 color;
varying vec4 var_color;

void main() { 
	gl_Position = mv_mat * vec4(position, 1.0);
	var_color = color;
}

uniform mat4 mv_mat; 
attribute vec3 position;

void main() { 
	gl_Position = mv_mat * vec4(position + vec3(0.0, 0.1, 0.0), 1.0);
}
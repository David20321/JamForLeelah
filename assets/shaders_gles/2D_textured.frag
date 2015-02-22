precision lowp float;

uniform sampler2D texture_id;
varying vec2 var_uv;

void main() { 
   gl_FragColor = texture2D(texture_id, var_uv);
}
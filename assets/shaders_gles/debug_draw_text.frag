precision lowp float;

uniform sampler2D texture_id;

varying vec2 var_uv;

void main() { 
   gl_FragColor = vec4(vec3(1.0), texture2D(texture_id, var_uv).r);
}
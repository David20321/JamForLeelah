precision lowp float;

uniform sampler2D texture_id;
varying vec3 var_view_pos;
varying vec2 var_uv;
varying vec3 var_normal;

vec4 ApplyFog(vec4 color) {
    vec3 fog_color = vec3(0.5,0.5,0.5);
	float depth = length(var_view_pos);
	return vec4(mix(color.xyz, fog_color, max(0.0, min(1.0, (depth - 10.0) * 0.1))), color.a);
}

void main() { 
    gl_FragColor = texture2D(texture_id, var_uv);
    //gl_FragColor = var_normal * 0.5 + vec3(0.5);
    gl_FragColor = ApplyFog(gl_FragColor);
}
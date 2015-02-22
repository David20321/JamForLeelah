precision lowp float;

uniform sampler2D texture_id;
uniform vec4 color;
uniform float energy;
varying vec3 var_view_pos;
varying vec2 var_uv;
varying vec3 var_normal;

vec4 ApplyFog(vec4 color) {
    vec3 fog_color = vec3(0.5,0.5,0.5);
	float depth = length(var_view_pos);
	return vec4(mix(color.xyz, fog_color, max(0.0, min(1.0, (depth - 10.0) * 0.1))), color.a);
}

vec4 ApplyLighting(vec4 color, vec3 normal) {
	return vec4(color.xyz * vec3(mix((normal.y+1.0)*0.5, 1.0, 0.5)), color.a);
}

void main() { 
   gl_FragColor = texture2D(texture_id, var_uv);
   gl_FragColor *= color;
   gl_FragColor = ApplyLighting(gl_FragColor, var_normal);
   gl_FragColor = ApplyFog(gl_FragColor);
   //outputColor.xyz = var_normal * 0.5 + vec3(0.5); 
}
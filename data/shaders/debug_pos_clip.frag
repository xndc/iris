#version 330 core

// Debug fragment shader for visualising clip-space positions.

uniform vec2 FramebufferSize;

layout(location = 0) out vec4 OutColor;

void main() {
	vec3 p = gl_FragCoord.xyz / vec3(FramebufferSize, 1.0);
	p.z = 0.5 * ((1.0 - p.x) + (1.0 - p.y));
	OutColor = vec4(p, 1.0);
}

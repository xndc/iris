#version 300 es
precision highp float;

// Should match locations and names defined in graphics/defaults.hh
layout (location = 0) in vec3 Position;
layout (location = 3) in vec2 Texcoord0;

uniform mat4 LocalToClip;

out vec4 ClipPos;
out vec2 VTexcoord0;

void main() {
	ClipPos = LocalToClip * vec4(Position, 1.0);
	gl_Position = ClipPos;
	VTexcoord0 = Texcoord0;
}

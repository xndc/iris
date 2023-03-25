#version 300 es
precision highp float;

// Vertex shader for full-screen passes. Maps X/Z coordinates into clip space X/Y.
// Intended to be used with a Y-facing quad, i.e. with the following positions:
//   -1.0,  0.0, -1.0,
//   -1.0,  0.0,  1.0,
//    1.0,  0.0, -1.0,
//    1.0,  0.0,  1.0,
// Output vertices will be on the Z=1 plane, in front of everything else in a reverse-Z setup.

layout(location = 0) in vec3 Position;

// Position of fragment coordinate within viewport, 0 to 1. Lets fragment shaders avoid having to
// compute this themselves. Equivalent to gl_FragCoord.xyz / vec3(FramebufferSize, 1.0).
out vec2 FragCoord01;

void main() {
	gl_Position = vec4(Position.xz, 1.0, 1.0);
	FragCoord01 = Position.xz * 0.5 + 0.5;
}

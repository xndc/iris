#version 330 core

// Should match locations and names defined in graphics/defaults.hh
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec4 Tangent;
layout (location = 3) in vec2 Texcoord0;
layout (location = 4) in vec2 Texcoord1;
layout (location = 5) in vec3 Color;
layout (location = 6) in vec3 Joints;
layout (location = 7) in vec3 Weights;

uniform mat4 MatModelViewProjection;
uniform mat4 MatModel;

out vec4 ClipPos;
out vec3 WorldPos;
out vec2 VTexcoord0;
out vec2 VTexcoord1;
out mat3 TangentBasisNormal;

void main() {
	WorldPos = (MatModel * vec4(Position, 1.0)).xyz;
	ClipPos = MatModelViewProjection * vec4(Position, 1.0);
	gl_Position = ClipPos;

	VTexcoord0 = Texcoord0;
	VTexcoord1 = Texcoord1;

	vec3 tangent = normalize((MatModel * vec4(Tangent.xyz, 0.0) / Tangent.w).xyz);
	vec3 normal = normalize((MatModel * vec4(Normal, 0.0)).xyz);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 basis = cross(normal, tangent);
	TangentBasisNormal = mat3(tangent, basis, normal);
}

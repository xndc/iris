#version 330 core

in vec2 FragCoord01;

uniform vec2 FramebufferSize;
uniform sampler2D RTAlbedo;
uniform sampler2D RTNormal;
uniform sampler2D RTMaterial;
uniform sampler2D RTVelocity;
uniform sampler2D RTDepth;
uniform vec3 LightPosition;
uniform vec3 LightColor;

uniform mat4 LocalToWorld;
uniform mat4 LocalToClip;
uniform mat4 LastLocalToClip;

layout(location = 0) out vec4 OutColorHDR;

void main() {
	ivec2 fragcoord = ivec2(gl_FragCoord.xy);
	vec3 albedo   = texelFetch(RTAlbedo,   fragcoord, 0).rgb;
	vec3 normal   = texelFetch(RTNormal,   fragcoord, 0).rgb;
	vec3 material = texelFetch(RTMaterial, fragcoord, 0).rgb;
	vec2 velocity = texelFetch(RTVelocity, fragcoord, 0).rg;
	float depth   = texelFetch(RTDepth,    fragcoord, 0).r;

	float occlusion = material.r;
	float roughness = max(material.g, 0.1); // zero breaks something in the lighting calculations
	float metalness = material.b;

	OutColorHDR = vec4(albedo, 1.0);
}

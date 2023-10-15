#pragma once
#include "base/base.hh"
#include "base/hash.hh"
#include "graphics/opengl.hh"

namespace Attributes {
	struct Item {
		GLuint index;
		const char* name;
		const char* gltf_name;
		constexpr Item(GLuint index, const char* name, const char* gltf_name):
			index{index}, name{name}, gltf_name{gltf_name} {}
	};

	static constexpr Item Position  = {0, "Position",  "POSITION"};
	static constexpr Item Normal    = {1, "Normal",    "NORMAL"};
	static constexpr Item Tangent   = {2, "Tangent",   "TANGENT"};
	static constexpr Item Texcoord0 = {3, "Texcoord0", "TEXCOORD_0"};
	static constexpr Item Texcoord1 = {4, "Texcoord1", "TEXCOORD_1"};
	static constexpr Item Color     = {5, "Color",     "COLOR_0"};
	static constexpr Item Joints    = {6, "Joints",    "JOINTS_0"};
	static constexpr Item Weights   = {7, "Weights",   "WEIGHTS_0"};

	static constexpr Item all[] = {
		Position, Normal, Tangent, Texcoord0, Texcoord1, Color, Joints, Weights
	};
}

namespace Uniforms {
	struct Item {
		uint64_t hash;
		const char* name;
		constexpr Item(const char* name) : hash{Hash64(name)}, name{name} {}
	};

	// Global parameters
	static constexpr Item Time = {"Time"};
	static constexpr Item FramebufferSize = {"FramebufferSize"};

	// Render targets from previous passes
	static constexpr Item RTAlbedo = {"RTAlbedo"};
	static constexpr Item RTNormal = {"RTNormal"};
	static constexpr Item RTMaterial = {"RTMaterial"};
	static constexpr Item RTVelocity = {"RTVelocity"};
	static constexpr Item RTColorHDR = {"RTColorHDR"};
	static constexpr Item RTPersistTAA = {"RTPersistTAA"};
	static constexpr Item RTDepth = {"RTDepth"};
	static constexpr Item RTDebugVis = {"RTDebugVis"};

	// Transformation matrices
	static constexpr Item LocalToWorld = {"LocalToWorld"};
	static constexpr Item LocalToClip = {"LocalToClip"};
	static constexpr Item LastLocalToClip = {"LastLocalToClip"};
	static constexpr Item ClipToWorld = {"ClipToWorld"};
	static constexpr Item ClipToView = {"ClipToView"};

	// Material sampler bindings
	static constexpr Item TexAlbedo    = {"TexAlbedo"};
	static constexpr Item TexNormal    = {"TexNormal"};
	static constexpr Item TexOcclusion = {"TexOcclusion"};
	static constexpr Item TexOccRghMet = {"TexOccRghMet"};

	// Material constant factors
	static constexpr Item ConstAlbedo       = {"ConstAlbedo"};
	static constexpr Item ConstMetallic     = {"ConstMetallic"};
	static constexpr Item ConstRoughness    = {"ConstRoughness"};
	static constexpr Item StippleHardCutoff = {"StippleHardCutoff"};
	static constexpr Item StippleSoftCutoff = {"StippleSoftCutoff"};

	// Shadow sampler and parameters
	static constexpr Item ShadowMap = {"ShadowMap"};
	static constexpr Item ShadowWorldToClip = {"ShadowWorldToClip"};
	static constexpr Item ShadowBiasMin = {"ShadowBiasMin"};
	static constexpr Item ShadowBiasMax = {"ShadowBiasMax"};
	static constexpr Item ShadowPCFTapsX = {"ShadowPCFTapsX"};
	static constexpr Item ShadowPCFTapsY = {"ShadowPCFTapsY"};

	// Camera parameters
	static constexpr Item CameraPosition = {"CameraPosition"};

	// Lighting parameters
	static constexpr Item LightPosition = {"LightPosition"};
	static constexpr Item LightColor = {"LightColor"};

	// Tonemap & PostFX parameters
	static constexpr Item TonemapExposure = {"TonemapExposure"};

	static constexpr Item all[] = {
		Time, FramebufferSize,
		RTAlbedo, RTNormal, RTMaterial, RTVelocity, RTColorHDR, RTPersistTAA, RTDepth, RTDebugVis,
		LocalToWorld, LocalToClip, LastLocalToClip, ClipToWorld, ClipToView,
		TexAlbedo, TexNormal, TexOcclusion, TexOccRghMet,
		ConstAlbedo, ConstMetallic, ConstRoughness, StippleHardCutoff, StippleSoftCutoff,
		ShadowMap, ShadowWorldToClip, ShadowBiasMin, ShadowPCFTapsX, ShadowPCFTapsY,
		CameraPosition,
		LightPosition, LightColor,
		TonemapExposure,
	};
}

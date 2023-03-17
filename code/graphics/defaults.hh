#pragma once

#include "base/base.hh"
#include "graphics/opengl.hh"

namespace DefaultAttributes {
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

namespace DefaultUniforms {
	struct Item {
		const char* name;
		constexpr Item(const char* name) : name{name} {}
	};

	// Global parameters
	static constexpr Item FramebufferSize = {"FramebufferSize"};

	// Render targets from previous passes
	static constexpr Item RTDiffuse = {"RTDiffuse"};
	static constexpr Item RTNormal = {"RTNormal"};
	static constexpr Item RTMaterial = {"RTMaterial"};
	static constexpr Item RTVelocity = {"RTVelocity"};
	static constexpr Item RTColorHDR = {"RTColorHDR"};
	static constexpr Item RTPersistTAA = {"RTPersistTAA"};
	static constexpr Item RTDepth = {"RTDepth"};

	// Transformation matrices
	static constexpr Item MatModelViewProjection = {"MatModelViewProjection"};
	static constexpr Item InvModelViewProjection = {"InvModelViewProjection"};
	static constexpr Item MatModel = {"MatModel"};
	static constexpr Item InvModel = {"InvModel"};

	// Material sampler bindings
	static constexpr Item TexAlbedo    = {"TexAlbedo"};
	static constexpr Item TexNormal    = {"TexNormal"};
	static constexpr Item TexOcclusion = {"TexOcclusion"};
	static constexpr Item TexOccRghMet = {"TexOccRghMet"};

	// Material constant factors
	static constexpr Item ConstAlbedo    = {"ConstAlbedo"};
	static constexpr Item ConstMetallic  = {"ConstMetallic"};
	static constexpr Item ConstRoughness = {"ConstRoughness"};

	static constexpr Item all[] = {
		RTDiffuse, RTNormal, RTMaterial, RTVelocity, RTColorHDR, RTDepth,
		FramebufferSize,
		MatModelViewProjection, InvModelViewProjection, MatModel, InvModel,
	};
}

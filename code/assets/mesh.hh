#pragma once
#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"
#include "graphics/opengl.hh"

struct BufferUsage {
	enum Enum : uint8_t {
		Vertex,
		Index,
	} v;
	FORCEINLINE constexpr BufferUsage(uint8_t v = 0) : v{Enum(v)} {};

	FORCEINLINE constexpr GLenum GLTarget() const {
		switch (v) {
			case BufferUsage::Vertex: return GL_ARRAY_BUFFER;
			case BufferUsage::Index: return GL_ELEMENT_ARRAY_BUFFER;
		} Unreachable();
	}
};

struct ElementType {
	enum Enum : uint8_t {
		SCALAR,
		VEC2,
		VEC3,
		VEC4,
		MAT2X2,
		MAT3X3,
		MAT4X4,
		Count,
	} v;
	FORCEINLINE constexpr ElementType(uint8_t v = 0) : v{Enum(v)} {};

	constexpr uint8_t Components() const {
		switch (v) {
			case SCALAR: return 1;
			case VEC2:   return 2;
			case VEC3:   return 3;
			case VEC4:   return 4;
			case MAT2X2: return 4;
			case MAT3X3: return 9;
			case MAT4X4: return 16;
			case Count:  Unreachable();
		} Unreachable();
	}
	constexpr const char* GLTFType() const {
		switch (v) {
			case SCALAR: return "SCALAR";
			case VEC2:   return "VEC2";
			case VEC3:   return "VEC3";
			case VEC4:   return "VEC4";
			case MAT2X2: return "MAT2";
			case MAT3X3: return "MAT3";
			case MAT4X4: return "MAT4";
			case Count:  Unreachable();
		}; Unreachable();
	}
	static ElementType FromGLTFType(const char* gltf) {
		for (uint8_t i = 0; i < Count; i++) {
			if (strcmp(gltf, ElementType(i).GLTFType()) == 0) {
				return ElementType(i);
			}
		}
		Panic("Invalid GLTF element type %s", gltf);
	}
};


struct ComponentType {
	enum Enum {
		I8,
		U8,
		I16,
		U16,
		I32,
		U32,
		F32,
		Count
	} v;
	FORCEINLINE constexpr ComponentType(uint8_t v = 0) : v{Enum(v)} {};

	constexpr uint8_t BytesPerComponent() const {
		switch (v) {
			case I8:  return 1;
			case U8:  return 1;
			case I16: return 2;
			case U16: return 2;
			case I32: return 4;
			case U32: return 4;
			case F32: return 4;
			case Count: Unreachable();
		} Unreachable();
	}
	constexpr GLenum GLEnum() const {
		switch (v) {
			case I8:  return GL_BYTE;
			case U8:  return GL_UNSIGNED_BYTE;
			case I16: return GL_SHORT;
			case U16: return GL_UNSIGNED_SHORT;
			case I32: return GL_INT;
			case U32: return GL_UNSIGNED_INT;
			case F32: return GL_FLOAT;
			case Count: Unreachable();
		} Unreachable();
	}
	constexpr const char* Name() const {
		switch (v) {
			case I8:  return "I8";
			case U8:  return "U8";
			case I16: return "I16";
			case U16: return "U16";
			case I32: return "I32";
			case U32: return "U32";
			case F32: return "F32";
			case Count: Unreachable();
		} Unreachable();
	}
	static constexpr ComponentType FromGLEnum(GLenum gl) {
		for (uint8_t i = 0; i < Count; i++) {
			if (gl == ComponentType(i).GLEnum()) {
				return ComponentType(i);
			}
		}
		Panic("Invalid GL component type %u", gl);
	}
};

struct PrimitiveType {
	enum Enum {
		POINTS,
		LINES,
		TRIANGLES,
		TRI_STRIP,
		Count,
	} v;
	FORCEINLINE constexpr PrimitiveType(uint8_t v = 0) : v{Enum(v)} {};

	uint8_t Vertices() const {
		switch (v) {
			case POINTS:    return 1;
			case LINES:     return 2;
			case TRIANGLES: return 3;
			case TRI_STRIP: return 3;
			case Count:     Unreachable();
		} Unreachable();
	}
	uint8_t Stride() const {
		switch (v) {
			case POINTS:    return 1;
			case LINES:     return 2;
			case TRIANGLES: return 3;
			case TRI_STRIP: return 1;
			case Count:     Unreachable();
		} Unreachable();
	}
	ElementType ElementType() const {
		switch (v) {
			case POINTS:    return ElementType::SCALAR;
			case LINES:     return ElementType::VEC2;
			case TRIANGLES: return ElementType::VEC3;
			case TRI_STRIP: return ElementType::VEC3;
			case Count:     Unreachable();
		} Unreachable();
	}
	GLenum GLEnum() const {
		switch (v) {
			case POINTS:    return GL_POINTS;
			case LINES:     return GL_LINES;
			case TRIANGLES: return GL_TRIANGLES;
			case TRI_STRIP: return GL_TRIANGLE_STRIP;
			case Count:     Unreachable();
		} Unreachable();
	}
	static constexpr PrimitiveType FromGLEnum(GLenum gl) {
		for (uint8_t i = 0; i < Count; i++) {
			if (gl == PrimitiveType(i).GLEnum()) {
				return PrimitiveType(i);
			}
		}
		Panic("Invalid GL component type %u", gl);
	}
};

struct Buffer {
	enum UsageFlags : uint8_t {
		// This buffer is intended for vertex data and will be bound to GL_ARRAY_BUFFER.
		USAGE_VERTEX_BUFFER = (1 << 0),
		// This buffer is intended for indices and will be bound to GL_ELEMENT_ARRAY_BUFFER.
		USAGE_INDEX_BUFFER  = (1 << 1),
	};

	// Flags indicating what this buffer is intended for.
	UsageFlags usage;

	// What kind of elements the buffer contains. Can be SCALAR, VEC3, MAT4X4 and so on.
	ElementType etype;

	// What kind of components each element in the buffer contains. Can be U8, U32, F32 and so on.
	ComponentType ctype;

	// How many elements the buffer contains.
	uint32_t elements;

	// Block of CPU-side memory for this buffer, if one exists.
	uint8_t* cpu_buffer = nullptr;

	// OpenGL handle for this buffer's GPU-side copy, if one exists.
	GLuint gpu_handle = 0;

	// Has this buffer been uploaded to the GPU?
	bool loaded = false;

	// Compute the total number of components this buffer contains.
	FORCEINLINE constexpr uint32_t Components() const { return elements * etype.Components(); }

	// Compute the total size of this buffer in bytes.
	FORCEINLINE constexpr uint32_t Size() const { return Components() * ctype.BytesPerComponent(); }

	// Get the OpenGL target that this buffer can be bound to.
	FORCEINLINE constexpr GLenum GLTarget() const {
		switch (usage & (USAGE_VERTEX_BUFFER | USAGE_INDEX_BUFFER)) {
			case USAGE_VERTEX_BUFFER: return GL_ARRAY_BUFFER;
			case USAGE_INDEX_BUFFER: return GL_ELEMENT_ARRAY_BUFFER;
		} Unreachable();
	}
};

struct Mesh {
	// What kind of primitive the mesh is comprised of. Can be TRIANGLES, TRI_STRIP and so on.
	PrimitiveType ptype;

	// Vertex and index buffers.
	Buffer vertex;
	Buffer index;

	// OpenGL Vertex Array Object for this mesh.
	GLuint gl_vertex_array = 0;

	// Axis-aligned bounding box for this mesh. Assumed not to exist if half-extents are all zero.
	vec3 aabb_half_extents = vec3(0);
	vec3 aabb_center = vec3(0);
};

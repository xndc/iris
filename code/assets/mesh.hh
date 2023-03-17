#pragma once
#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"
#include "graphics/opengl.hh"
#include "graphics/defaults.hh"

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

	constexpr uint8_t components() const {
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
	constexpr const char* gltf_type() const {
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
	static ElementType from_gltf_type(const char* gltf) {
		for (uint8_t i = 0; i < Count; i++) {
			if (strcmp(gltf, ElementType(i).gltf_type()) == 0) {
				return ElementType(i);
			}
		}
		Panic("Invalid GLTF element type %s", gltf);
	}
};


struct ComponentType {
	enum Enum : uint8_t {
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

	constexpr uint8_t bytes() const {
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
	constexpr GLenum gl_enum() const {
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
	constexpr const char* name() const {
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
	static constexpr ComponentType from_gl_enum(GLenum gl) {
		for (uint8_t i = 0; i < Count; i++) {
			if (gl == ComponentType(i).gl_enum()) {
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

	constexpr uint8_t vertices() const {
		switch (v) {
			case POINTS:    return 1;
			case LINES:     return 2;
			case TRIANGLES: return 3;
			case TRI_STRIP: return 3;
			case Count:     Unreachable();
		} Unreachable();
	}
	constexpr uint8_t stride() const {
		switch (v) {
			case POINTS:    return 1;
			case LINES:     return 2;
			case TRIANGLES: return 3;
			case TRI_STRIP: return 1;
			case Count:     Unreachable();
		} Unreachable();
	}
	constexpr ElementType element_type() const {
		switch (v) {
			case POINTS:    return ElementType::SCALAR;
			case LINES:     return ElementType::VEC2;
			case TRIANGLES: return ElementType::VEC3;
			case TRI_STRIP: return ElementType::VEC3;
			case Count:     Unreachable();
		} Unreachable();
	}
	constexpr const char* name() const {
		switch (v) {
			case POINTS:    return "POINTS";
			case LINES:     return "LINES";
			case TRIANGLES: return "TRIANGLES";
			case TRI_STRIP: return "TRI_STRIP";
			case Count:     Unreachable();
		} Unreachable();
	}
	constexpr GLenum gl_enum() const {
		switch (v) {
			case POINTS:    return GL_POINTS;
			case LINES:     return GL_LINES;
			case TRIANGLES: return GL_TRIANGLES;
			case TRI_STRIP: return GL_TRIANGLE_STRIP;
			case Count:     Unreachable();
		} Unreachable();
	}
	static constexpr PrimitiveType from_gl_enum(GLenum gl) {
		for (uint8_t i = 0; i < Count; i++) {
			if (gl == PrimitiveType(i).gl_enum()) {
				return PrimitiveType(i);
			}
		}
		Panic("Invalid GL component type %u", gl);
	}
};

struct BufferUsage {
	enum Enum : uint8_t {
		// This buffer is unused or its usage type is not known yet.
		Unknown,
		// This buffer is intended for vertex data and will be bound to GL_ARRAY_BUFFER.
		Vertex,
		// This buffer is intended for indices and will be bound to GL_ELEMENT_ARRAY_BUFFER.
		Index,
	} v;
	FORCEINLINE constexpr BufferUsage(uint8_t v = 0) : v{Enum(v)} {};

	FORCEINLINE constexpr GLenum gl_target() const {
		switch (v) {
			case BufferUsage::Unknown: return GL_ARRAY_BUFFER;
			case BufferUsage::Vertex: return GL_ARRAY_BUFFER;
			case BufferUsage::Index: return GL_ELEMENT_ARRAY_BUFFER;
		} Unreachable();
	}
};

struct Buffer {
	BufferUsage usage = BufferUsage::Unknown;
	uint32_t size = 0;
	// Block of CPU-side memory for this buffer, if one exists.
	uint8_t* cpu_buffer = nullptr;
	// OpenGL handle for this buffer's GPU-side copy, if one exists.
	GLuint gpu_handle = 0;
	// Has this buffer been uploaded to the GPU? (Not the same thing as gpu_handle != 0)
	bool loaded = false;
};

struct BufferView {
	Buffer* buffer;
	ElementType etype;
	ComponentType ctype;
	uint32_t elements;
	// Offset into buffer at which this BufferView starts, in bytes.
	uint32_t offset = 0;

	FORCEINLINE constexpr BufferView() {}
	FORCEINLINE constexpr BufferView(Buffer* buffer, ElementType etype, ComponentType ctype, uint32_t elements):
		buffer{buffer}, etype{etype}, ctype{ctype}, elements{elements} {}

	FORCEINLINE constexpr uint32_t components() const { return elements * etype.components(); }
	FORCEINLINE constexpr uint32_t size() const { return components() * ctype.bytes(); }
};

struct Mesh {
	PrimitiveType ptype;

	static constexpr size_t MaxVertexAttribs = CountOf(DefaultAttributes::all);
	// One BufferView, which may be pointing to a valid or null buffer, for each vertex attribute.
	BufferView vertex_attribs [MaxVertexAttribs];

	BufferView index_buffer;
	GLuint gl_vertex_array = 0;

	// Axis-aligned bounding box for this mesh. Assumed not to exist if half-extents are all zero.
	vec3 aabb_half_extents = vec3(0);
	vec3 aabb_center = vec3(0);
};

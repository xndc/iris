#include "assets/mesh.hh"

constexpr BufferView::BufferView(Buffer* buffer, ElementType etype, ComponentType ctype, uint32_t elements):
	buffer{buffer}, etype{etype}, ctype{ctype}, elements{elements}
{
	if (buffer->usage.v == BufferUsage::Index &&
		!(ctype.v == ComponentType::U8 || ctype.v == ComponentType::U16 || ctype.v == ComponentType::U32))
	{
		LOG_F(WARNING, "BufferView %p (Buffer %p) is meant for indices, but uses unsupported ctype %s",
			this, buffer, ctype.name());
	}
}

Buffer* Buffer::upload() {
	if (loaded) {
		return this;
	}
	if (ExpectFalse(cpu_buffer == nullptr)) {
		Panic("Called Buffer::upload() without a staged cpu_buffer");
	}
	if (gpu_handle == 0) {
		glGenBuffers(1, &gpu_handle);
	}
	glBindBuffer(usage.gl_target(), gpu_handle);
	glBufferData(usage.gl_target(), size, cpu_buffer, GL_STATIC_DRAW);
	glBindBuffer(usage.gl_target(), 0);
	cpu_buffer = nullptr;
	loaded = true;
	return this;
}

bool Mesh::compute_aabb() {
	BufferView& position = vertex_attribs[Attributes::Position.index];
	if (!position.buffer) {
		return false;
	}
	vec3 aabb_min = { INFINITY,  INFINITY,  INFINITY};
	vec3 aabb_max = {-INFINITY, -INFINITY, -INFINITY};
	// TODO: Support other formats for the position buffer
	if (position.etype.v == ElementType::VEC3 || position.ctype.v == ComponentType::F32) {
		for (size_t i = 0; i < position.elements; i++) {
			uint8_t* p = &position.buffer->cpu_buffer[position.offset + 3 * i * sizeof(float)];
			vec3* vp = reinterpret_cast<vec3*>(p); // NOTE: needs -fno-strict-aliasing
			aabb_min = vec3(Min(aabb_min.x, vp->x), Min(aabb_min.y, vp->y), Min(aabb_min.z, vp->z));
			aabb_max = vec3(Max(aabb_max.x, vp->x), Max(aabb_max.y, vp->y), Max(aabb_max.z, vp->z));
		}
	} else {
		LOG_F(WARNING, "Can't compute mesh AABB for %s/%s", position.etype.gltf_type(), position.ctype.name());
		return false;
	}
	aabb_center = (aabb_min + aabb_max) / vec3(2.0f);
	aabb_half_extents = aabb_max - aabb_center;
	return true;
}

Mesh* Mesh::upload() {
	glGenVertexArrays(1, &gl_vertex_array);
	glBindVertexArray(gl_vertex_array);
	if (index_buffer.buffer) {
		Buffer& buffer = *index_buffer.buffer;
		buffer.upload();
		glBindBuffer(buffer.usage.gl_target(), buffer.gpu_handle);
	}
	for (uint32_t iattr = 0; iattr < MaxVertexAttribs; iattr++) {
		if (vertex_attribs[iattr].buffer) {
			BufferView& bufview = vertex_attribs[iattr];
			Buffer& buffer = *bufview.buffer;
			buffer.upload();
			glBindBuffer(buffer.usage.gl_target(), buffer.gpu_handle);
			GLuint location = Attributes::all[iattr].index;
			glEnableVertexAttribArray(location);
			glVertexAttribPointer(location, bufview.etype.components(), bufview.ctype.gl_enum(), false,
				bufview.stride(), reinterpret_cast<const GLvoid*>(bufview.offset));
		}
	}
	glBindVertexArray(0);
	return this;
}

namespace Meshes {
	Mesh QuadXZ;
	Mesh Cube;
}

void CreateMeshes() {
	/* QuadXZ */ {
		Mesh& mesh = Meshes::QuadXZ;
		const float positions[] = {
			-1.0f, 0.0f, -1.0f,
			-1.0f, 0.0f,  1.0f,
			 1.0f, 0.0f, -1.0f,
			 1.0f, 0.0f,  1.0f,
		};
		const uint16_t indices[] = {
			0, 2, 1,
			1, 2, 3,
		};
		mesh.ptype = PrimitiveType::TRIANGLES;
		mesh.vertex_attribs[Attributes::Position.index] = BufferView(
			new Buffer(BufferUsage::Vertex, sizeof(positions), &positions),
			ElementType::VEC3, ComponentType::F32, CountOf(positions) / 3);
		mesh.index_buffer = BufferView(
			new Buffer(BufferUsage::Index, sizeof(indices), &indices),
			ElementType::VEC3, ComponentType::U16, CountOf(indices) / 3);
		mesh.compute_aabb();
		mesh.upload();
	}

	/* Cube */ {
		Mesh& mesh = Meshes::Cube;
		const float positions[] = {
			// Front
			-0.5, -0.5,  0.5,
			 0.5, -0.5,  0.5,
			 0.5,  0.5,  0.5,
			-0.5,  0.5,  0.5,
			// Top
			-0.5,  0.5,  0.5,
			 0.5,  0.5,  0.5,
			 0.5,  0.5, -0.5,
			-0.5,  0.5, -0.5,
			// Back
			 0.5, -0.5, -0.5,
			-0.5, -0.5, -0.5,
			-0.5,  0.5, -0.5,
			 0.5,  0.5, -0.5,
			// Bottom
			-0.5, -0.5, -0.5,
			 0.5, -0.5, -0.5,
			 0.5, -0.5,  0.5,
			-0.5, -0.5,  0.5,
			// Left
			-0.5, -0.5, -0.5,
			-0.5, -0.5,  0.5,
			-0.5,  0.5,  0.5,
			-0.5,  0.5, -0.5,
			// Right
			 0.5, -0.5,  0.5,
			 0.5, -0.5, -0.5,
			 0.5,  0.5, -0.5,
			 0.5,  0.5,  0.5,
		};
		const float texcoords[] = {
			0.0, 0.0,
			1.0, 0.0,
			1.0, 1.0,
			0.0, 1.0,
		};
		const uint16_t indices[] = {
			// Front
			0,  1,  2,
			2,  3,  0,
			// Top
			4,  5,  6,
			6,  7,  4,
			// Back
			8,  9,  10,
			10, 11, 8,
			// bottom
			12, 13, 14,
			14, 15, 12,
			// Left
			16, 17, 18,
			18, 19, 16,
			// Right
			20, 21, 22,
			22, 23, 20,
		};
		mesh.ptype = PrimitiveType::TRIANGLES;
		mesh.vertex_attribs[Attributes::Position.index] = BufferView(
			new Buffer(BufferUsage::Vertex, sizeof(positions), &positions),
			ElementType::VEC3, ComponentType::F32, CountOf(positions) / 3);
		mesh.vertex_attribs[Attributes::Texcoord0.index] = BufferView(
			new Buffer(BufferUsage::Vertex, sizeof(texcoords), &texcoords),
			ElementType::VEC2, ComponentType::F32, CountOf(texcoords) / 2);
		mesh.index_buffer = BufferView(
			new Buffer(BufferUsage::Index, sizeof(indices), &indices),
			ElementType::VEC3, ComponentType::U16, CountOf(indices) / 3);
		mesh.compute_aabb();
		mesh.upload();
	}
}

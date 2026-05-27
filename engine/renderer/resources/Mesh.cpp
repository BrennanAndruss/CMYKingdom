#include "renderer/resources/Mesh.h"

#include <algorithm>
#include <stdexcept>

namespace engine
{
	Mesh::Mesh(const std::vector<glm::vec3>& positions,
			   const std::vector<glm::vec3>& normals,
			   const std::vector<glm::vec2>& texcoords,
			   const std::vector<unsigned int>& indices)
	{
		setupMesh(positions, normals, texcoords, indices);
		_bbox = BBox::fromPositions(positions);
	}

	Mesh::Mesh(const std::vector<glm::vec3>& positions,
			   const std::vector<glm::vec3>& normals,
			   const std::vector<glm::vec2>& texcoords,
			   const std::vector<unsigned int>& indices,
			   const std::vector<glm::uvec4>& boneIds,
			   const std::vector<glm::vec4>& boneWeights)
	{
		setupMesh(positions, normals, texcoords, indices, boneIds, boneWeights);
		_bbox = BBox::fromPositions(positions);
	}

	void Mesh::setupMesh(const std::vector<glm::vec3>& positions,
						 const std::vector<glm::vec3>& normals,
						 const std::vector<glm::vec2>& texcoords,
						 const std::vector<unsigned int>& indices)
	{
		_isSkinned = false;

		glGenVertexArrays(1, &_vao);
		glBindVertexArray(_vao);

		glGenBuffers(1, &_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);

		glGenBuffers(1, &_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

		std::size_t numVertices = positions.size();
		_vertBuf.resize(numVertices * VERTEX_SIZE);

		for (std::size_t i = 0; i < numVertices; ++i)
		{
			std::size_t vi = i * VERTEX_SIZE;

			_vertBuf[vi++] = positions[i].x;
			_vertBuf[vi++] = positions[i].y;
			_vertBuf[vi++] = positions[i].z;

			_vertBuf[vi++] = normals[i].x;
			_vertBuf[vi++] = normals[i].y;
			_vertBuf[vi++] = normals[i].z;

			_vertBuf[vi++] = texcoords[i].x;
			_vertBuf[vi++] = texcoords[i].y;
		}

		glBufferData(GL_ARRAY_BUFFER, _vertBuf.size() * sizeof(float), _vertBuf.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		std::size_t stride = VERTEX_SIZE * sizeof(float);
		std::size_t offset = 0;

		glEnableVertexAttribArray(static_cast<GLuint>(Attrib::Position));
		glVertexAttribPointer(static_cast<GLuint>(Attrib::Position), 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
		offset += 3 * sizeof(float);

		glEnableVertexAttribArray(static_cast<GLuint>(Attrib::Normal));
		glVertexAttribPointer(static_cast<GLuint>(Attrib::Normal), 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
		offset += 3 * sizeof(float);

		glEnableVertexAttribArray(static_cast<GLuint>(Attrib::TexCoord));
		glVertexAttribPointer(static_cast<GLuint>(Attrib::TexCoord), 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);

		glBindVertexArray(0);

		_numIndices = indices.size();
	}

	void Mesh::setupMesh(const std::vector<glm::vec3>& positions,
					 const std::vector<glm::vec3>& normals,
					 const std::vector<glm::vec2>& texcoords,
					 const std::vector<unsigned int>& indices,
					 const std::vector<glm::uvec4>& boneIds,
					 const std::vector<glm::vec4>& boneWeights)
	{
		if (positions.size() != boneIds.size() || positions.size() != boneWeights.size())
		{
			throw std::runtime_error("Skinned mesh vertex data size mismatch");
		}

		setupMesh(positions, normals, texcoords, indices);

		_isSkinned = true;
		_boneIdBuf = boneIds;
		_boneWeightBuf = boneWeights;

		glBindVertexArray(_vao);

		glGenBuffers(1, &_boneIdVbo);
		glBindBuffer(GL_ARRAY_BUFFER, _boneIdVbo);
		glBufferData(GL_ARRAY_BUFFER, _boneIdBuf.size() * sizeof(glm::uvec4), _boneIdBuf.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(static_cast<GLuint>(Attrib::BoneIds));
		glVertexAttribIPointer(static_cast<GLuint>(Attrib::BoneIds), 4, GL_UNSIGNED_INT, sizeof(glm::uvec4), (void*)0);

		glGenBuffers(1, &_boneWeightVbo);
		glBindBuffer(GL_ARRAY_BUFFER, _boneWeightVbo);
		glBufferData(GL_ARRAY_BUFFER, _boneWeightBuf.size() * sizeof(glm::vec4), _boneWeightBuf.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(static_cast<GLuint>(Attrib::BoneWeights));
		glVertexAttribPointer(static_cast<GLuint>(Attrib::BoneWeights), 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0); // this 

		glBindVertexArray(0);
	}

	void Mesh::draw() const
	{
		glBindVertexArray(_vao);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_numIndices), GL_UNSIGNED_INT, (void*)0);
		glBindVertexArray(0);
	}
}
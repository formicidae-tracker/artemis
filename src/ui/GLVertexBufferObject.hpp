#pragma once

#include <GL/glew.h>
#include <GL/gl.h>

#include <Eigen/Core>

#include <memory>

namespace fort {
namespace artemis {

class GLVertexBufferObject {
public:
	typedef std::shared_ptr<GLVertexBufferObject> Ptr;
	typedef Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> Matrix;


	GLVertexBufferObject();
	virtual ~GLVertexBufferObject();

	void Clear();

	void Upload(const Matrix & data,
	            size_t d_vertexSize,
	            size_t d_texelSize,
	            size_t d_colorSize,
	            bool staticUpload = false);

	void Render(GLenum type) const;

private:
	GLuint d_ID;

	size_t d_vertexSize,d_texelSize,d_colorSize,d_elementSize;

};

}  // namespace artemis
}  // namespace fort

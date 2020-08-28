#include "GLVertexBufferObject.hpp"


namespace fort {
namespace artemis {


GLVertexBufferObject::GLVertexBufferObject() {
	glGenBuffers(1,&d_ID);
}

GLVertexBufferObject::~GLVertexBufferObject() {
	glDeleteBuffers(1,&d_ID);
}

void GLVertexBufferObject::Clear() {
	d_elementSize = 0;
}

void GLVertexBufferObject::Upload(const Matrix & data,
                                  size_t vertexSize,
                                  size_t texelSize,
                                  size_t colorSize) {
	if ( vertexSize + texelSize + colorSize != data.cols() ) {

		throw std::invalid_argument("Data (rows:"
		                            + std::to_string(data.rows())
		                            + ", cols:"
		                            + std::to_string(data.cols())
		                            + ") does not match memory layout Vertex:"
		                            + std::to_string(vertexSize)
		                            + " Tex: "
		                            + std::to_string(texelSize)
		                            + " Color: "
		                            + std::to_string(colorSize));
	}

	d_elementSize = data.rows();

	d_vertexSize = vertexSize;
	d_texelSize = texelSize;
	d_colorSize = colorSize;

	glBindBuffer(GL_ARRAY_BUFFER,d_ID);
	glBufferData(GL_ARRAY_BUFFER,sizeof(float)*data.rows()*data.cols(),data.data(),GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER,0);

}

void GLVertexBufferObject::Render(GLenum mode) {
	auto colSize = d_vertexSize + d_texelSize + d_colorSize;

	if ( d_elementSize == 0 || colSize == 0) {
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER,d_ID);
	if ( d_vertexSize > 0 ) {
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0,
		                      d_vertexSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)(0));
	}

	if ( d_texelSize > 0 ) {
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1,
		                      d_texelSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)(d_vertexSize * sizeof(GLfloat)));
	}

	if ( d_colorSize > 0 ) {
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2,
		                      d_colorSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)( ( d_vertexSize + d_texelSize ) * sizeof(GLfloat)));
	}

	glDrawArrays(mode,0,d_elementSize);

	if ( d_colorSize > 0 ) {
		glDisableVertexAttribArray(2);
	}
	if ( d_texelSize > 0 ) {
		glDisableVertexAttribArray(1);
	}
	if ( d_vertexSize > 0 ) {
		glDisableVertexAttribArray(0);
	}

	glBindBuffer(GL_ARRAY_BUFFER,0);

}

}  // artemis
}  // fort

#include "GLVertexBufferObject.hpp"

#include <iostream>

static void glCheckError_(const char *file, int line){
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        std::string error;
        switch (errorCode)
        {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::cerr << error << " | " << file << " (" << line << ")" << std::endl;
    }
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)



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

void GLVertexBufferObject::UploadRect(const cv::Rect & bBox, bool staticUpload) {
	GLVertexBufferObject::Matrix boxData(6,2);
	boxData <<
		bBox.x             , bBox.y              ,
		bBox.x + bBox.width, bBox.y              ,
		bBox.x             , bBox.y + bBox.height,
		bBox.x             , bBox.y + bBox.height,
		bBox.x + bBox.width, bBox.y              ,
		bBox.x + bBox.width, bBox.y + bBox.height;

	Upload(boxData,2,0,0,staticUpload);
}

void GLVertexBufferObject::Upload(const Matrix & data,
                                  size_t vertexSize,
                                  size_t texelSize,
                                  size_t colorSize,
                                  bool staticUpload ) {
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
	glCheckError();
	glBufferData(GL_ARRAY_BUFFER,sizeof(float)*data.rows()*data.cols(),data.data(),staticUpload ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
	glCheckError();
	glBindBuffer(GL_ARRAY_BUFFER,0);
	glCheckError();

}

void GLVertexBufferObject::Render(GLenum mode) const {
	auto colSize = d_vertexSize + d_texelSize + d_colorSize;

	if ( d_elementSize == 0 || colSize == 0) {
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER,d_ID);

	if ( d_vertexSize > 0 ) {
		glEnableVertexAttribArray(0);
		glCheckError();
		glVertexAttribPointer(0,
		                      d_vertexSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)(0));
		glCheckError();
	}

	if ( d_texelSize > 0 ) {
		glEnableVertexAttribArray(1);
		glCheckError();
		glVertexAttribPointer(1,
		                      d_texelSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)(d_vertexSize * sizeof(GLfloat)));
		glCheckError();
	}

	if ( d_colorSize > 0 ) {
		glEnableVertexAttribArray(2);
		glCheckError();
		glVertexAttribPointer(2,
		                      d_colorSize,
		                      GL_FLOAT,
		                      GL_FALSE,
		                      colSize * sizeof(GLfloat),
		                      (void*)( ( d_vertexSize + d_texelSize ) * sizeof(GLfloat)));
		glCheckError();
	}

	glDrawArrays(mode,0,d_elementSize);
	glCheckError();
	if ( d_colorSize > 0 ) {
		glDisableVertexAttribArray(2);
		glCheckError();
	}
	if ( d_texelSize > 0 ) {
		glDisableVertexAttribArray(1);
		glCheckError();
	}
	if ( d_vertexSize > 0 ) {
		glDisableVertexAttribArray(0);
		glCheckError();
	}

	glBindBuffer(GL_ARRAY_BUFFER,0);
		glCheckError();

}

}  // artemis
}  // fort

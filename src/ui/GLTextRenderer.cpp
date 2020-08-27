#include "GLTextRenderer.hpp"

#include <Eigen/Core>

#include "ShaderUtils.hpp"

#include "ui/shaders_data.h"

#include <glog/logging.h>

namespace fort {
namespace artemis {


GLTextRenderer::GLTextRenderer(const GLAsciiFontAtlas::Ptr & atlas,
                               const cv::Size & workingSize,
                               const cv::Vec4f & foreground,
                               const cv::Vec4f & background)
	: d_atlas(atlas)
	, d_workingSize(workingSize)
	, d_foreground(foreground)
	, d_background(background) {
	glGenBuffers(1,&d_vbo);
	InitializeProgram();
}

GLTextRenderer::~GLTextRenderer() {
	glDeleteBuffers(1,&d_vbo);
}

void GLTextRenderer::Compile(const std::vector<std::pair<std::string,cv::Point>> & texts,
                             const cv::Size & inputSize) {
	std::vector<GLfloat> data;

	auto ratio =  1.0 / std::min(float(d_workingSize.width)/float(inputSize.width),
	                             float(d_workingSize.height)/float(inputSize.height));
	const auto & totalGlyphSize= d_atlas->TotalGlyphSize();
	float gwidth = totalGlyphSize.width * ratio;
	float gheight = totalGlyphSize.height * ratio;

	for ( const auto & [text,pos] : texts ) {
		auto cPos = pos;
		for ( const auto & c : text) {
			const GLfloat * UV = d_atlas->CharUV(c);
			data.push_back(cPos.x);
			data.push_back(cPos.y);
			data.push_back(UV[0]);
			data.push_back(UV[1]);

			data.push_back(cPos.x);
			data.push_back(cPos.y + gheight);
			data.push_back(UV[6]);
			data.push_back(UV[7]);

			data.push_back(cPos.x + gwidth);
			data.push_back(cPos.y);
			data.push_back(UV[2]);
			data.push_back(UV[3]);

			data.push_back(cPos.x);
			data.push_back(cPos.y + gheight);
			data.push_back(UV[6]);
			data.push_back(UV[7]);

			data.push_back(cPos.x + gwidth);
			data.push_back(cPos.y);
			data.push_back(UV[2]);
			data.push_back(UV[3]);


			data.push_back(cPos.x + gwidth);
			data.push_back(cPos.y + gheight);
			data.push_back(UV[4]);
			data.push_back(UV[5]);
			cPos.x += gwidth;
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER,d_vbo);
	glBufferData(GL_ARRAY_BUFFER,sizeof(GLfloat) * data.size(),&data[0],GL_DYNAMIC_DRAW);
	d_vertexSize = data.size()/4;
}


std::shared_ptr<GLuint> GLTextRenderer::s_program;

void GLTextRenderer::Render(const cv::Rect & roi ) {
	Eigen::Matrix3f scaleMat;

	scaleMat << 2.0f / float(roi.width), 0.0f,  -1.0 - roi.x / float(roi.width),
		0.0f, -2.0f / float(roi.height) , 1.0f + roi.y / float(roi.height),
		0.0f,0.0f,0.0f;

	glUseProgram(*s_program);

	auto scaleMatID = glGetUniformLocation(*s_program,"scaleMat");
	glUniformMatrix3fv(scaleMatID,1,GL_FALSE,scaleMat.data());

	auto foregroundID = glGetUniformLocation(*s_program,"foreground");
	glUniform4fv(foregroundID,1,&d_foreground[0]);
	auto backgroundID = glGetUniformLocation(*s_program,"background");
	glUniform4fv(backgroundID,1,&d_background[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,d_atlas->TextureID());
	glUniform1i(d_atlas->TextureID(), 0);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, d_vbo);
	glVertexAttribPointer(0,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      4*sizeof(GLfloat),
	                      (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      4*sizeof(GLfloat),
	                      (GLvoid*)(2*sizeof(GLfloat)));


	glDrawArrays(GL_TRIANGLES,0,d_vertexSize);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);


}

void GLTextRenderer::InitializeProgram() {
	if ( s_program ) {
		return;
	}

	s_program = std::make_shared<GLuint>(ShaderUtils::CompileProgram(std::string(font_vertexshader,font_vertexshader+font_vertexshader_size),
	                                                                std::string(font_fragmentshader,font_fragmentshader+font_fragmentshader_size)));

}

} // namespace artemis
} // namespace fort

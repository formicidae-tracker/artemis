#pragma once

#include <GL/glew.h>

#include "GLAsciiFontAtlas.hpp"

namespace fort {
namespace artemis {


class GLTextRenderer {
public:
	typedef std::shared_ptr<GLTextRenderer> Ptr;

	GLTextRenderer(const GLAsciiFontAtlas::Ptr & atlas,
	               const cv::Size & workingSize,
	               const cv::Vec4f & foreground,
	               const cv::Vec4f & background);
	~GLTextRenderer();

	void Compile(const std::vector<std::pair<std::string,cv::Point>> & text,
	             const cv::Size & inputSize);

	void Render(const cv::Rect & roi);

private:
	static void InitializeProgram();

	static std::shared_ptr<GLuint> s_program;

	GLAsciiFontAtlas::Ptr d_atlas;
	GLuint                d_vbo;
	cv::Size              d_workingSize;
	cv::Vec4f             d_foreground,d_background;
	size_t                d_vertexSize;
};

} // namespace artemis
} // namespace fort

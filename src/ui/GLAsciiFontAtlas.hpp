#pragma once

#include <GL/glew.h>
#include <GL/gl.h>

#include <opencv2/core.hpp>

#include <memory>

namespace fort {
namespace artemis {

class GLAsciiFontAtlas {
public:
	typedef std::shared_ptr<GLAsciiFontAtlas> Ptr;

	static Ptr VGAFont();


	GLAsciiFontAtlas(const cv::Mat & texture,
	                 const cv::Size & totalGlyphSize,
	                 const cv::Size & grid,
	                 std::vector<uint8_t> charSet = {});
	~GLAsciiFontAtlas();

	GLuint TextureID() const;

	const cv::Size & TotalGlyphSize() const;

	const GLfloat * CharUV(unsigned char c);

	cv::Size TextSize(const std::string & text) const;

private:

	GLfloat  d_UVs[256*4*2];
	GLfloat *d_indexes[256];
	GLuint   d_textureID;
	cv::Size d_totalGlyphSize;
};

} // namespace artemis
} // namespace fort

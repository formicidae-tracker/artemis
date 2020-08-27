#include "GLAsciiFontAtlas.hpp"

#include "ImageTextRenderer.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <glog/logging.h>

namespace fort {
namespace artemis {


GLAsciiFontAtlas::Ptr GLAsciiFontAtlas::VGAFont() {
	cv::Mat texture(256,256,CV_8UC3);
	texture.setTo(cv::Vec3b(0,0,0));
	size_t i = -1;
	for ( int r = 0; r < 12; ++r ) {
		std::string toPrint;
		for (size_t j = 0; j < 22; ++j) {
			char c = ++i >= 256 ? 0x00 : i;
			toPrint.push_back(c);
		}
		ImageTextRenderer::RenderText(texture,toPrint,cv::Point(1,r*16));
	}
	cv::Mat gray;
	cv::cvtColor(texture,gray,cv::COLOR_BGR2GRAY);

	cv::imwrite("/tmp/foo.png",gray);

	return std::make_shared<GLAsciiFontAtlas>(gray,cv::Size(9,16),cv::Size(22,12));
}


GLAsciiFontAtlas::GLAsciiFontAtlas(const cv::Mat & texture,
                                   const cv::Size & totalGlyphSize,
                                   const cv::Size & grid,
                                   std::vector<uint8_t> charSet )
	: d_totalGlyphSize(totalGlyphSize) {
	glGenTextures(1,&d_textureID);
	glBindTexture(GL_TEXTURE_2D,d_textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D,0,GL_R8,texture.cols,texture.rows,0,GL_RED,GL_UNSIGNED_BYTE,texture.data);

	if ( charSet.empty() ) {
		for ( size_t i = 0; i < 256; ++i) {
			charSet.push_back(i);
		}
	}

	for ( size_t i = 0; i < 256; ++i ) {
		d_indexes[i] = &d_UVs[0];
	}

	size_t i = -1;
	for ( const auto c : charSet ) {
		size_t iy = ++i / grid.width;
		size_t ix = i - grid.width * iy;
		cv::Point corners[4] = { { int((ix + 0) * totalGlyphSize.width), int((iy + 0) * totalGlyphSize.height)},
		                         { int((ix + 1) * totalGlyphSize.width), int((iy + 0) * totalGlyphSize.height)},
		                         { int((ix + 1) * totalGlyphSize.width), int((iy + 1) * totalGlyphSize.height)},
		                         { int((ix + 0) * totalGlyphSize.width), int((iy + 1) * totalGlyphSize.height)}};

		d_UVs[2*4*i + 0] = float(corners[0].x) / float(texture.cols);
		d_UVs[2*4*i + 1] = float(corners[0].y) / float(texture.rows);
		d_UVs[2*4*i + 2] = float(corners[1].x) / float(texture.cols);
		d_UVs[2*4*i + 3] = float(corners[1].y) / float(texture.rows);
		d_UVs[2*4*i + 4] = float(corners[2].x) / float(texture.cols);
		d_UVs[2*4*i + 5] = float(corners[2].y) / float(texture.rows);
		d_UVs[2*4*i + 6] = float(corners[3].x) / float(texture.cols);
		d_UVs[2*4*i + 7] = float(corners[3].y) / float(texture.rows);

		d_indexes[c] = &d_UVs[2*4*i];
	}


}

GLAsciiFontAtlas::~GLAsciiFontAtlas() {
	glDeleteTextures(1,&d_textureID);
}


GLuint GLAsciiFontAtlas::TextureID() const {
	return d_textureID;
}

const cv::Size & GLAsciiFontAtlas::TotalGlyphSize() const {
	return d_totalGlyphSize;
}

const GLfloat * GLAsciiFontAtlas::CharUV(unsigned char c) {
	return d_indexes[c];
}

cv::Size GLAsciiFontAtlas::TextSize(const std::string & text) const {
	return cv::Size(text.size() * d_totalGlyphSize.width,d_totalGlyphSize.height);
}

} // namespace artemis
} // namespace fort

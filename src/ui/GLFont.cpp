#include "GLFont.hpp"

#include <fontconfig/fontconfig.h>


namespace fort {
namespace artemis {

class FontConfig {
public:
	FontConfig() {
		d_config = FcInitLoadConfigAndFonts();
	}

	~FontConfig() {
		FcConfigDestroy(d_config);
	}

	std::string Locate(const std::string & name) {
		auto pattern = std::shared_ptr<FcPattern>(FcNameParse((FcChar8*)(name.c_str())),
		                                          &FcPatternDestroy);
		FcConfigSubstitute(d_config,pattern.get(),FcMatchPattern);
		FcDefaultSubstitute(pattern.get());
		FcResult result;
		FcPattern* font = FcFontMatch(d_config, pattern.get(), &result);
		FcChar8 * file = NULL;
		if ( font && FcPatternGetString(font,FC_FILE,0,&file) == FcResultMatch ) {
			return std::string((char*)(file));
		}
		throw std::runtime_error("Could not find font '" + name + "'");
	}

private:
	FcConfig * d_config;
};




GLFont::GLFont(const std::string & fontname, size_t fontSize, size_t textureSize) {
	static FontConfig fc;
	auto fontfile = fc.Locate(fontname);
	d_atlas = texture_atlas_new(textureSize,textureSize,1);
	d_font = texture_font_new_from_file(d_atlas,fontSize,fontfile.c_str());

	//load all ASCII char
	static std::string asciiChar = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%&*(){}[]_-+=?<>,.";
	texture_font_load_glyphs(d_font,asciiChar.c_str());

	glGenTextures(1,&d_atlas->id);
	glBindTexture( GL_TEXTURE_2D, d_atlas->id );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, d_atlas->width, d_atlas->height,
                  0, GL_RED, GL_UNSIGNED_BYTE, d_atlas->data );

}

GLFont::~GLFont() {
	glDeleteTextures(1,&d_atlas->id);
	texture_font_delete(d_font);
	texture_atlas_delete(d_atlas);
}

GLuint GLFont::TextureID() const {
	return d_atlas->id;
}



cv::Rect GLFont::UploadText(GLVertexBufferObject & dest,
                            double scalingFactor,
                            const PositionedText & text) {
	GLVertexBufferObject::Matrix data(6* (text.first.size()+1),4);
	auto block = data.block(0,0,6*(text.first.size()+1),4);
	auto [tlx,tly,brx,bry] = RenderTextInMatrix(block,scalingFactor,text);
	dest.Upload(data,2,2,0);
	return cv::Rect(cv::Point(tlx,tly),cv::Size(brx-tlx,bry-tly));
}

std::tuple<float,float,float,float>
GLFont::RenderTextInMatrix(Eigen::Block<GLVertexBufferObject::Matrix> & block,
                           double scalingFactor,
                           const PositionedText & text) {

	using namespace Eigen;
	Vector2f pos(text.second.x,text.second.y);

	Vector2f bottomLeft(pos),topRight(pos);
	float margin = -1.0;
	float advanceY = d_font->ascender - d_font->descender + d_font->linegap;
	for ( 	size_t i = 0; i < text.first.size(); ++i ) {
		if ( text.first[i] == '\n' ) {
			pos.x() = text.second.x;
			pos.y() += advanceY;
			continue;
		}

		auto glyph = texture_font_get_glyph(d_font,&(text.first[i]));
		if (glyph == NULL ) {
			continue;
		}


		Vector2f p0 = pos + scalingFactor * Vector2f(glyph->offset_x,-glyph->offset_y);
		Vector2f p1 = p0 + scalingFactor * Vector2f(glyph->width,glyph->height);
		Vector2d s0(glyph->s0,glyph->t0);
		Vector2d s1(glyph->s1,glyph->t1);
		bottomLeft.y() = std::max(bottomLeft.y(),p1.y());
		topRight.y() = std::min(topRight.y(),p0.y());


		block.block<6,4>(6*(i+1),0) <<
			p0.x(), p0.y(),s0.x(),s0.y(),
			p0.x(), p1.y(),s0.x(),s1.y(),
			p1.x(), p0.y(),s1.x(),s0.y(),
			p0.x(), p1.y(),s0.x(),s1.y(),
			p1.x(), p1.y(),s1.x(),s1.y(),
			p1.x(), p0.y(),s1.x(),s0.y();

		pos.x() += scalingFactor * glyph->advance_x;

		margin = std::max(margin, glyph->advance_x);
	}
	margin *= scalingFactor * 0.25;
	bottomLeft.x() -= margin;
	bottomLeft.y() += margin;

	topRight.x() = pos.x() + margin;
	topRight.y() -= margin;

	block.block<6,4>(0,0) <<
		bottomLeft.x(),bottomLeft.y(),0.0f,0.0f,
		topRight.x(),bottomLeft.y(),0.0f,0.0f,
		topRight.x(),topRight.y(),0.0f,0.0f,
		bottomLeft.x(),bottomLeft.y(),0.0f,0.0f,
		topRight.x(),topRight.y(),0.0f,0.0f,
		bottomLeft.x(),topRight.y(),0.0f,0.0f;

	return {bottomLeft.x(),topRight.y(),topRight.x(),bottomLeft.y()};
}


} // namespace fort
} // namespace artemis

#pragma once

#include <freetype-gl.h>

#include <memory>
#include <string>
#include <algorithm>
#include <numeric>

#include <opencv2/core.hpp>

#include "GLVertexBufferObject.hpp"

namespace fort {
namespace artemis {

class GLVertexBufferObject;

class GLFont {
public :

	typedef std::shared_ptr<GLFont> Ptr;
	typedef std::pair<std::string,cv::Point> PositionedText;
	GLFont(const std::string & fontname, size_t fontSize, size_t textureSize);
	~GLFont();

	GLuint TextureID() const;


	cv::Rect UploadText(GLVertexBufferObject & dest,
	                    double scalingFactor,
	                    const PositionedText & text);

	template <typename Iter>
	cv::Rect UploadTexts(GLVertexBufferObject & dest,
	                     double scalingFactor,
	                     const Iter & begin,
	                     const Iter & end) {
		Eigen::Vector2f topLeft(std::numeric_limits<float>::max(),std::numeric_limits<float>::max()),bottomRight(0.0f,0.0f);

		auto totalQuads = std::accumulate(begin,end,
		                                  size_t(0),
		                                  [](size_t cur, const PositionedText & txt) {  return cur + txt.first.size()+1; });

		GLVertexBufferObject::Matrix data(totalQuads * 6,4);
		size_t row = 0;
		std::for_each(begin,
		              end,
		              [&topLeft,&bottomRight,&row,&data,scalingFactor,this](const PositionedText & text) {

			              size_t dataSize =  (text.first.size() + 1 )* 6;
			              auto block = data.block(row,0,dataSize,4);
			              auto [tlx,tly,brx,bry] = RenderTextInMatrix(block,scalingFactor,text);
			              row += dataSize;
			              topLeft = Eigen::Vector2f(std::min(topLeft.x(),tlx),std::min(topLeft.y(),tly));
			              bottomRight = Eigen::Vector2f(std::max(bottomRight.x(),brx),std::min(bottomRight.y(),bry));
		              });
		dest.Upload(data,2,2,0);
		bottomRight -= topLeft;
		return cv::Rect(cv::Point(topLeft.x(),topLeft.y()),cv::Size(bottomRight.x(),bottomRight.y()));
	}
private :
std::tuple<float,float,float,float> RenderTextInMatrix(Eigen::Block<GLVertexBufferObject::Matrix> & block,
                                                       double scalingFactor,
                                                       const PositionedText & text);

	texture_atlas_t * d_atlas;
	texture_font_t  * d_font;
};

} // namespace fort
} // namespace artemis

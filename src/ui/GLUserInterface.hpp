#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <memory>

#include "UserInterface.hpp"

#include "GLTextRenderer.hpp"
#include "GLVertexBufferObject.hpp"

namespace fort {
namespace artemis {

class GLFont;

class GLUserInterface : public UserInterface {
public:
	GLUserInterface(const cv::Size & workingResolution,
	                const cv::Size & fullResolution,
	                const Options & options,
	                const ROIChannelPtr & roiChannel);

	virtual ~GLUserInterface();

	void PollEvents() override;

	void UpdateFrame(const FrameToDisplay & frame,
	                 const DataToDisplay & data) override;

private:
	typedef std::unique_ptr<GLFWwindow,void(*)(GLFWwindow*)> GLFWwindowPtr;
	GLFWwindowPtr d_window;

	static GLFWwindowPtr OpenWindow(const cv::Size & size);

	struct DrawBuffer {
		FrameToDisplay            Frame;
		DataToDisplay             Data;
		cv::Size                  TrackingSize;
		bool                      FullUploaded;
		GLuint                    PBO;
		GLVertexBufferObject::Ptr NormalTags,HighlightedTags,TagLabels;
	};


	void SetWindowCallback();
	void InitContext();
	void InitGLData();
	void InitPBOs();


	void OnSizeChanged(int width, int height);
	void OnKey(int key, int scancode, int action, int mods);
	void OnText(unsigned int codepoint);
	void OnMouseMove(double x, double y);
	void OnMouseInput(int button, int action, int mods);
	void OnScroll(double xOffset,double yOffset);

	void Zoom(int increment);
	void Displace(const Eigen::Vector2f & offset);
	void UpdateROI(const cv::Rect & ROI);


	void ComputeViewport();

	void Draw();
	void UploadTexture(DrawBuffer & buffer);
	void UploadPoints(DrawBuffer & buffer);

	void Draw(const DrawBuffer & buffer);

	void DrawMovieFrame(const DrawBuffer & buffer);
	void DrawPoints(const DrawBuffer & buffer);
	void DrawLabels(const DrawBuffer & buffer);
	void DrawInformations(const DrawBuffer & buffer);
	void DrawWatermark();
	void DrawHelp();
	void DrawPrompt();

	static void UploadMatrix(GLuint programID,
	                         const std::string & name,
	                         const Eigen::Matrix3f & matrix);

	static void UploadColor(GLuint programID,
	                        const std::string & name,
	                        const cv::Vec3f & color);

	static void UploadColor(GLuint programID,
	                        const std::string & name,
	                        const cv::Vec4f & color);

	void RenderText(const GLVertexBufferObject & buffer,
	                const GLFont & font,
	                const cv::Rect & inputSize,
	                const cv::Vec4f & forgeround,
	                const cv::Vec4f & background);

	static void ComputeRectVertices(GLVertexBufferObject::Matrix & mat,
	                         const cv::Rect & rect);

	static void ComputeProjection(const cv::Rect & roi,Eigen::Matrix3f & res);
	void UpdateProjections();

	float FullToWindowScaleFactor() const;

	DrawBuffer     d_buffer[2];
	size_t         d_index;

	const cv::Size d_workingSize,d_fullSize;
	cv::Size       d_windowSize,d_viewSize;
	int            d_currentScaleFactor;
	cv::Point      d_currentPOI;
	cv::Rect       d_ROI;

	Eigen::Matrix3f d_fullProjection,d_viewProjection,d_roiProjection;

	std::shared_ptr<GLFont> d_labelFont,d_overlayFont,d_watermarkFont;
	cv::Size                d_overlayGlyphSize;
	cv::Rect                d_watermarkBox;

	GLVertexBufferObject::Ptr d_frameVBO,
		d_boxOverlayVBO,d_textOverlayVBO,
		d_watermarkVBO,d_helpVBO,d_promptVBO;
	GLuint d_frameProgram,d_frameTexture,
		d_pointProgram,d_primitiveProgram, d_fontProgram;

	static const cv::Vec4f OVERLAY_GLYPH_FOREGROUND;
	static const cv::Vec4f OVERLAY_GLYPH_BACKGROUND;
	static const cv::Vec4f OVERLAY_BACKGROUND;
	static const cv::Vec4f LABEL_FOREGROUND;
	static const cv::Vec4f LABEL_BACKGROUND;

	const static size_t OVERLAY_COLS = 30;
	const static size_t OVERLAY_ROWS = 8;
	const static size_t NORMAL_POINT_SIZE = 70;
	const static size_t HIGHLIGHTED_POINT_SIZE = 100;
	const static size_t LABEL_FONT_SIZE = 16;
	const static size_t OVERLAY_FONT_SIZE = 14;
};


} // namespace artemis
} // namespace fort

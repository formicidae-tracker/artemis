#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <memory>

#include "UserInterface.hpp"

#include "GLTextRenderer.hpp"


namespace fort {
namespace artemis {

class GLUserInterface : public UserInterface {
public:
	GLUserInterface(const cv::Size & workingResolution,
	                const DisplayOptions & options,
	                const ZoomChannelPtr & zoomChannel);

	virtual ~GLUserInterface();

	void PollEvents() override;

	void UpdateFrame(const FrameToDisplay & frame,
	                 const DataToDisplay & data) override;

private:
	typedef std::unique_ptr<GLFWwindow,void(*)(GLFWwindow*)> GLFWwindowPtr;
	GLFWwindowPtr d_window;

	static GLFWwindowPtr OpenWindow(const cv::Size & size);

	struct DrawBuffer {
		FrameToDisplay      Frame;
		DataToDisplay       Data;
		GLuint              PBO;
		GLuint              NormalPointsVBO,HighlightedPointsVBO;
		GLTextRenderer::Ptr TagLabels;
	};


	void SetWindowCallback();
	void InitContext();
	void InitGLData();
	void InitPBOs();

	void ComputeViewport();

	void OnSizeChanged(int width, int height);

	void Draw();

	void UploadTexture(const DrawBuffer & buffer);
	void UploadPoints(DrawBuffer & buffer);
	void Draw(const DrawBuffer & buffer);


	void DrawMovieFrame(const DrawBuffer & buffer);
	void DrawPoints(const DrawBuffer & buffer);
	void DrawLabels(const DrawBuffer & buffer);
	void DrawInformations(const DrawBuffer & buffer);

	DrawBuffer     d_buffer[2];
	size_t         d_index;

	const cv::Size d_workingSize;
	cv::Size       d_windowSize;
	Zoom           d_zoom;

	GLAsciiFontAtlas::Ptr d_vgaFont;
	GLTextRenderer::Ptr   d_dataRenderer;
	GLuint d_frameVBO,d_frameTBO,d_frameProgram,d_frameTexture,
		d_pointProgram,d_pointVBO,d_dataOverlayVBO,d_primitiveProgram;

	const static size_t OVERLAY_COLS = 30;
	const static size_t OVERLAY_ROWS = 8;
};


} // namespace artemis
} // namespace fort

#pragma once


#include <GLFW/glfw3.h>

#include <memory>

#include "UserInterface.hpp"


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
		FrameToDisplay Frame;
		DataToDisplay  Data;
		GLuint         PBO;
	};


	void SetWindowCallback();
	void InitContext();
	void InitGLData();
	void InitPBOs();

	void ComputeViewport();

	void OnSizeChanged(int width, int height);

	void Draw();

	void UploadTexture(const DrawBuffer & buffer);

	void Draw(const DrawBuffer & buffer);


	DrawBuffer     d_buffer[2];
	size_t         d_index;

	const cv::Size d_workingSize;
	cv::Size       d_windowSize;
	Zoom           d_zoom;


	GLuint d_frameVBO,d_frameProgram,d_frameTexture;
};


} // namespace artemis
} // namespace fort

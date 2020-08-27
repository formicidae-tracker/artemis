
#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <Eigen/Core>


#include <glog/logging.h>

#include "GLUserInterface.hpp"

#include "ShaderUtils.hpp"

#include "ui/shaders_data.h"


#define throw_glfw_error(ctx) do {	  \
		const char * glfwErrorDescription; \
		glfwGetError(&glfwErrorDescription); \
		throw std::runtime_error(std::string(ctx) + ": " + glfwErrorDescription); \
	} while(0)




namespace fort {
namespace artemis {




GLUserInterface::GLUserInterface(const cv::Size & workingResolution,
                                 const DisplayOptions & options,
                                 const ZoomChannelPtr & zoomChannel)
	: UserInterface(workingResolution,options,zoomChannel)
	, d_window(nullptr,[](GLFWwindow*){})
	, d_index(0)
	, d_workingSize(workingResolution)
	, d_windowSize(workingResolution) {

	DLOG(INFO) << "[GLUserInterface]: initialiazing GLFW";

	if ( !glfwInit() ) {
		throw_glfw_error("could not initilaize GLFW");
	}

	try {
		d_window = OpenWindow(workingResolution);
	} catch ( const std::exception &  ) {
		glfwTerminate();
		throw;
	}


	try {
		InitContext();
	} catch ( const std::exception &) {
		d_window.reset();
		glfwTerminate();
		throw;
	}

	InitGLData();
	SetWindowCallback();

}

GLUserInterface::GLFWwindowPtr GLUserInterface::OpenWindow(const cv::Size & size) {
	DLOG(INFO) << "[GLUserInterface]: Opening window";

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_MAXIMIZED,GLFW_TRUE);
	auto window = glfwCreateWindow(size.width,
	                               size.height,
	                               "artemis",
	                               NULL,NULL);
	if ( !window ) {
		throw_glfw_error("could not open window");
	}

	return GLFWwindowPtr(window,[](GLFWwindow*){});
}

void GLUserInterface::SetWindowCallback() {
	glfwSetWindowUserPointer(d_window.get(),this);


	// forbids user to close window, only SIGINT should close artemis gracefully
	glfwSetWindowCloseCallback(d_window.get(),[](GLFWwindow * w) {
		                                          glfwSetWindowShouldClose(w,GLFW_FALSE);
	                                          });

	glfwSetFramebufferSizeCallback(d_window.get(),
	                          [](GLFWwindow * w, int width, int height) {
		                          static_cast<GLUserInterface*>(glfwGetWindowUserPointer(w))->OnSizeChanged(width,height);
	                          });

}

void GLUserInterface::InitContext() {
	DLOG(INFO) << "[GLUserInterface]: Making context current";

	glfwMakeContextCurrent(d_window.get());
	const GLenum err = glewInit();
	if ( err != GLEW_OK ) {
		throw std::runtime_error(std::string("Could not initialize GLEW: ") + (const char*)(glewGetErrorString(err)));
	}
}


GLUserInterface::~GLUserInterface() {
	d_window.reset();
	glfwTerminate();
}

void GLUserInterface::PollEvents() {
	glfwPollEvents();
}

void GLUserInterface::UpdateFrame(const FrameToDisplay & frame,
                                  const DataToDisplay & data)  {
	auto & buffer = d_buffer[d_index];
	buffer.Frame = frame;
	buffer.Data = data;
	d_index = (d_index + 1 )%2;
	Draw();
}

void GLUserInterface::ComputeViewport() {
	int wantedWidth = d_workingSize.width * double(d_windowSize.height) / double(d_workingSize.height);
	if ( wantedWidth <= d_windowSize.width ) {
		DLOG(INFO) << "[GLUserInterface]: Viewport with full height " << wantedWidth << "x" << d_windowSize.height;

		glViewport((d_windowSize.width - wantedWidth) / 2,
		           0,
		           wantedWidth,
		           d_windowSize.height);
	} else {
		wantedWidth = d_windowSize.width;
		int wantedHeight = d_workingSize.height * double(d_windowSize.width) / double(d_workingSize.width);
		DLOG(INFO) << "[GLUserInterface]: Viewport with full width " << d_windowSize.width << "x" << wantedHeight;
		glViewport(0,
		           (d_windowSize.height - wantedHeight) / 2,
		           d_windowSize.width,
		           wantedHeight);
	}
}

void GLUserInterface::OnSizeChanged(int width,int height) {
	DLOG(INFO) << "New framebuffer size " << width << "x" << height;

	d_windowSize = cv::Size(width,height);
	ComputeViewport();
	Draw();
}


void GLUserInterface::Draw() {
	auto & nextBuffer = d_buffer[(d_index+1)%2];
	const auto & currentBuffer = d_buffer[d_index];

	UploadTexture(nextBuffer);
	UploadPoints(nextBuffer);

	Draw(currentBuffer);
}

bool operator==(const UserInterface::Zoom & a,
                const UserInterface::Zoom & b) {
	return a.Scale == b.Scale && a.Center == b.Center;
}

void GLUserInterface::InitPBOs() {
	DLOG(INFO) << "[GLUserInterface]: initialiazing PBOs";
	static auto vgaFont = GLAsciiFontAtlas::VGAFont();
	for ( size_t i = 0; i < 2; ++i) {
		glGenBuffers(1,&(d_buffer[i].HighlightedPointsVBO));
		glGenBuffers(1,&(d_buffer[i].NormalPointsVBO));

		glGenBuffers(1,&(d_buffer[i].PBO));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER,d_buffer[i].PBO);
		glBufferData(GL_PIXEL_UNPACK_BUFFER,d_workingSize.width*d_workingSize.height,0,GL_STREAM_DRAW);
		d_buffer[i].TagLabels = std::make_shared<GLTextRenderer>(vgaFont,
		                                                         d_workingSize,
		                                                         cv::Vec4f(1.0f,1.0f,1.0f,1.0f),
		                                                         cv::Vec4f(0.0f,0.0f,0.0f,1.0f));
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
}

void GLUserInterface::UploadTexture(const DrawBuffer & buffer) {
	std::shared_ptr<cv::Mat> toUpload = buffer.Frame.Full;
	if ( buffer.Frame.Zoomed
	     && d_zoom == buffer.Frame.CurrentZoom ) {
		toUpload = buffer.Frame.Zoomed;
	}

	if ( !toUpload ) {
		return;
	}

	size_t frameSize = toUpload->cols * toUpload->rows;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer.PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER,frameSize,0,GL_STREAM_DRAW);
	auto ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY);
	if ( ptr != nullptr ) {
		memcpy(ptr,toUpload->data,frameSize);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
}

void GLUserInterface::InitGLData() {
	DLOG(INFO) << "[GLUserInterface]: InitGLData";
	InitPBOs();

	ComputeViewport();

	glClearColor(0.0f,0.0f,0.0f,1.0f);

	GLuint VAO;
	glGenVertexArrays(1,&VAO);
	glBindVertexArray(VAO);

	d_frameProgram = ShaderUtils::CompileProgram(std::string(frame_vertexshader,frame_vertexshader+frame_vertexshader_size),
	                                             std::string(frame_fragmentshader,frame_fragmentshader+frame_fragmentshader_size));

	static const GLfloat frameData[] = {
	                                    -1.0f, -1.0f,
	                                    1.0f, -1.0f,
	                                    1.0f,  1.0f,
	                                    1.0f,  1.0f,
	                                    -1.0f,  1.0f,
	                                    -1.0f,  -1.0f,
	};

	glGenBuffers(1,&d_frameVBO);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameVBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(frameData),frameData,GL_STATIC_DRAW);


	static const GLfloat frameUVData[] = {
	                                      0.0f, 1.0f,
	                                      1.0f, 1.0f,
	                                      1.0f,  0.0f,
	                                      1.0f,  0.0f,
	                                      0.0f,  0.0f,
	                                      0.0f,  1.0f,
	};

	glGenBuffers(1,&d_frameTBO);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameTBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(frameUVData),frameUVData,GL_DYNAMIC_DRAW);


	glGenTextures(1,&d_frameTexture);

	glBindTexture(GL_TEXTURE_2D, d_frameTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	d_pointProgram = ShaderUtils::CompileProgram(std::string(primitive_vertexshader,primitive_vertexshader+primitive_vertexshader_size),
	                                             std::string(circle_fragmentshader,circle_fragmentshader+circle_fragmentshader_size));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GLUserInterface::DrawMovieFrame(const DrawBuffer & buffer) {
	glUseProgram(d_frameProgram);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,d_frameTexture);
	glUniform1i(d_frameTexture, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer.PBO);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R8,d_workingSize.height,d_workingSize.width,0,GL_RED,GL_UNSIGNED_BYTE,0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameVBO);
	glVertexAttribPointer(0,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      0,
	                      (void*)0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameTBO);
	glVertexAttribPointer(1,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      0,
	                      (void*)0);


	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

}

std::string FormatTagID(uint32_t tagID) {
	std::ostringstream oss;
	oss << "0x" << std::setfill('0') << std::setw(3) << std::hex << tagID;
	return oss.str();
}

void GLUserInterface::UploadPoints(DrawBuffer & buffer) {
	if ( !buffer.Frame.Message ) {
		return;
	}
	std::vector<std::pair<std::string,cv::Point>> tagLabels;
	std::vector<GLfloat> pointData;
	size_t nPoints = std::max(buffer.Data.HighlightedIndexes.size(),buffer.Data.NormalIndexes.size());
	tagLabels.reserve(buffer.Data.HighlightedIndexes.size() + buffer.Data.NormalIndexes.size());
	pointData.reserve(2*nPoints);
	for ( const auto hIndex : buffer.Data.HighlightedIndexes ) {
		const auto & t = buffer.Frame.Message->tags(hIndex);
		pointData.push_back(t.x());
		pointData.push_back(t.y());
		tagLabels.push_back(std::make_pair(FormatTagID(t.id()),cv::Point(t.x(),t.y())));
	}

	glBindBuffer(GL_ARRAY_BUFFER,buffer.HighlightedPointsVBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(GLfloat) * pointData.size(),&pointData[0],GL_DYNAMIC_DRAW);

	pointData.clear();
	for ( const auto nIndex : buffer.Data.NormalIndexes ) {
		const auto & t = buffer.Frame.Message->tags(nIndex);
		pointData.push_back(t.x());
		pointData.push_back(t.y());
		tagLabels.push_back(std::make_pair(FormatTagID(t.id()),cv::Point(t.x(),t.y())));
	}

	glBindBuffer(GL_ARRAY_BUFFER,buffer.NormalPointsVBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(GLfloat) * pointData.size(),&pointData[0],GL_DYNAMIC_DRAW);

	buffer.TagLabels->Compile(tagLabels,cv::Size(buffer.Frame.Message->width(),
	                                             buffer.Frame.Message->height()));
}

void GLUserInterface::DrawPoints(const DrawBuffer & buffer) {
	if ( !buffer.Frame.Message ) {
		return;
	}
	glUseProgram(d_pointProgram);

	GLuint scaleID = glGetUniformLocation(d_pointProgram,"scaleMat");
	Eigen::Matrix3f scaleMat;
	scaleMat <<  2.0f / float(buffer.Frame.Message->width()),0,-1.0f,
		0, -2.0f / float(buffer.Frame.Message->height()),1.0f,
		0.0f,0.0f,0.0f;

	glUniformMatrix3fv(scaleID,1,GL_FALSE,scaleMat.data());

	GLuint colorID = glGetUniformLocation(d_pointProgram,"circleColor");
	glUniform3f(colorID,0.0f,1.0f,1.0f);
	glPointSize(18.0f);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffer.HighlightedPointsVBO);
	glVertexAttribPointer(0,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      0,
	                      (void*)0);

	glDrawArrays(GL_POINTS,0,buffer.Data.HighlightedIndexes.size());

	glUniform3f(colorID,1.0f,0.0f,0.0f);
	glPointSize(9.0f);

	glBindBuffer(GL_ARRAY_BUFFER, buffer.NormalPointsVBO);
	glVertexAttribPointer(0,
	                      2,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      0,
	                      (void*)0);

	glDrawArrays(GL_POINTS,0,buffer.Data.NormalIndexes.size());

	glDisableVertexAttribArray(0);

}

void GLUserInterface::DrawLabels(const DrawBuffer & buffer ) {
	static auto vgaFont = GLAsciiFontAtlas::VGAFont();
}


void GLUserInterface::Draw(const DrawBuffer & buffer ) {
	if ( !buffer.Frame.Full ) {
		return;
	}

	if ( d_zoom.Scale != 1.0 && !(buffer.Frame.CurrentZoom == d_zoom) ) {
		// TODO: update UV coordinates
	}
	glClear( GL_COLOR_BUFFER_BIT);

	DrawMovieFrame(buffer);
	DrawPoints(buffer);
	buffer.TagLabels->Render(cv::Rect(cv::Point(0,0),cv::Size(buffer.Frame.Message->width(),
	                                                          buffer.Frame.Message->height())));

	glfwSwapBuffers(d_window.get());
}



} // namespace artemis
} // namespace fort

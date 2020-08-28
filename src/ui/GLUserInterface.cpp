
#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <Eigen/Core>


#include <glog/logging.h>

#include "GLUserInterface.hpp"

#include "ShaderUtils.hpp"

#include "ui/shaders_data.h"

#include "GLFont.hpp"


#if (GLFW_VERSION_MAJOR * 100 + GLFW_VERSION_MINOR) < 303
#define IMPLEMENT_GLFW_GET_ERROR 1
#endif

#ifdef IMPLEMENT_GLFW_GET_ERROR
#include <mutex>
#include <deque>
#endif


#define throw_glfw_error(ctx) do {	  \
		const char * glfwErrorDescription; \
		glfwGetError(&glfwErrorDescription); \
		throw std::runtime_error(std::string(ctx) + ": " + glfwErrorDescription); \
	} while(0)


namespace fort {
namespace artemis {

#ifdef IMPLEMENT_GLFW_GET_ERROR

std::deque<std::pair<int,const char*>> GLFWErrorStack;
std::mutex  GLFWErrorMutex;
void GLFWErrorCallback(int error, const char * description) {
	std::lock_guard<std::mutex> lock(GLFWErrorMutex);
	GLFWErrorStack.push_back({error,description});
}

}
}

int glfwGetError(const char ** description) {
	std::lock_guard<std::mutex> lock(fort::artemis::GLFWErrorMutex);
	if ( fort::artemis::GLFWErrorStack.empty() ) {
		return 0;
	}
	auto res = fort::artemis::GLFWErrorStack.front();
	if ( description != nullptr ) {
		*description = res.second;
	}
	fort::artemis::GLFWErrorStack.pop_front();
	return res.first;
}

namespace fort {
namespace artemis {

#endif // IMPLEMENT_GLFW_GET_ERROR



GLUserInterface::GLUserInterface(const cv::Size & workingResolution,
                                 const DisplayOptions & options,
                                 const ZoomChannelPtr & zoomChannel)
	: UserInterface(workingResolution,options,zoomChannel)
	, d_window(nullptr,[](GLFWwindow*){})
	, d_index(0)
	, d_workingSize(workingResolution)
	, d_windowSize(workingResolution) {

#ifdef IMPLEMENT_GLFW_GET_ERROR
	glfwSetErrorCallback(&GLFWErrorCallback);
#endif // IMPLEMENT_GLFW_GET_ERROR
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
	d_boxOverlayVBO.reset();
	d_textOverlayVBO.reset();
	d_frameVBO.reset();
	for(size_t i = 0; i <2 ; ++i) {
		d_buffer[i].NormalTags.reset();
		d_buffer[i].HighlightedTags.reset();
		d_buffer[i].TagLabels.reset();
	}

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
	if ( !frame.Message ) {
		buffer.TrackingSize = cv::Size(0,0);
	} else {
		buffer.TrackingSize = cv::Size(frame.Message->width(),frame.Message->height());
	}
	d_index = (d_index + 1 )%2;
	Draw();
}

void GLUserInterface::ComputeViewport() {
	float windowRatio = float(d_windowSize.height) / float(d_workingSize.height);
	int wantedWidth = d_workingSize.width * windowRatio;
	if ( wantedWidth <= d_windowSize.width ) {
		DLOG(INFO) << "[GLUserInterface]: Viewport with full height " << wantedWidth << "x" << d_windowSize.height;

		glViewport((d_windowSize.width - wantedWidth) / 2,
		           0,
		           wantedWidth,
		           d_windowSize.height);
		d_viewSize = cv::Size(wantedWidth,d_windowSize.height);
	} else {
		wantedWidth = d_windowSize.width;
		windowRatio = float(d_windowSize.width) / float(d_workingSize.width);
		int wantedHeight = d_workingSize.height * windowRatio;
		DLOG(INFO) << "[GLUserInterface]: Viewport with full width " << d_windowSize.width << "x" << wantedHeight;
		glViewport(0,
		           (d_windowSize.height - wantedHeight) / 2,
		           d_windowSize.width,
		           wantedHeight);
		d_viewSize = cv::Size(d_windowSize.width,wantedHeight);
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
	for ( size_t i = 0; i < 2; ++i) {
		d_buffer[i].NormalTags = std::make_shared<GLVertexBufferObject>();
		d_buffer[i].HighlightedTags = std::make_shared<GLVertexBufferObject>();
		d_buffer[i].TagLabels = std::make_shared<GLVertexBufferObject>();

		glGenBuffers(1,&(d_buffer[i].PBO));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER,d_buffer[i].PBO);
		glBufferData(GL_PIXEL_UNPACK_BUFFER,d_workingSize.width*d_workingSize.height,0,GL_STREAM_DRAW);
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

	GLuint VAO;
	glGenVertexArrays(1,&VAO);
	glBindVertexArray(VAO);


	InitPBOs();

	ComputeViewport();

	glClearColor(0.0f,0.0f,0.0f,1.0f);


	d_frameProgram = ShaderUtils::CompileProgram(std::string(frame_vertexshader,frame_vertexshader+frame_vertexshader_size),
	                                             std::string(frame_fragmentshader,frame_fragmentshader+frame_fragmentshader_size));
	d_frameVBO = std::make_shared<GLVertexBufferObject>();


	GLVertexBufferObject::Matrix frameData(6,4);
	frameData <<
		-1.0f, -1.0f, 0.0f, 1.0f,
		+1.0f, -1.0f, 1.0f, 1.0f,
		+1.0f, +1.0f, 1.0f,  0.0f,
		+1.0f, +1.0f, 1.0f,  0.0f,
		-1.0f, +1.0f, 0.0f,  0.0f,
		-1.0f, -1.0f, 0.0f,  1.0f;
	d_frameVBO->Upload(frameData,2,2,0);

	glGenTextures(1,&d_frameTexture);

	glBindTexture(GL_TEXTURE_2D, d_frameTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	d_pointProgram = ShaderUtils::CompileProgram(std::string(primitive_vertexshader,primitive_vertexshader+primitive_vertexshader_size),
	                                             std::string(circle_fragmentshader,circle_fragmentshader+circle_fragmentshader_size));

	d_primitiveProgram = ShaderUtils::CompileProgram(std::string(primitive_vertexshader,primitive_vertexshader+primitive_vertexshader_size),
	                                                 std::string(primitive_fragmentshader,primitive_fragmentshader+primitive_fragmentshader_size));

	d_fontProgram = ShaderUtils::CompileProgram(std::string(font_vertexshader,font_vertexshader+font_vertexshader_size),
	                                            std::string(font_fragmentshader,font_fragmentshader+font_fragmentshader_size));
	size_t l = LABEL_FONT_SIZE;
	size_t o = OVERLAY_FONT_SIZE;


	d_overlayFont =  std::make_shared<GLFont>("Free Mono",o,512);
	d_boxOverlayVBO = std::make_shared<GLVertexBufferObject>();
	d_textOverlayVBO = std::make_shared<GLVertexBufferObject>();

	d_overlayGlyphSize = d_overlayFont->UploadText(*d_textOverlayVBO,
	                                               1.0,
	                                               {"0",cv::Point(0,0)}).size();

	auto & gSize = d_overlayGlyphSize;
	size_t cols = OVERLAY_COLS - 8;
	size_t rows = OVERLAY_ROWS + 2;

	glUseProgram(d_primitiveProgram);

	Eigen::Matrix3f scaleMat;
	scaleMat << 2.0 / float(d_workingSize.width), 0.0f , -1.0f,
		0.0f, -2.0 / float(d_workingSize.height), 1.0f,
		0.0f,0.0f,0.0f;

	auto scaleID = glGetUniformLocation(d_primitiveProgram,"scaleMat");
	glUniformMatrix3fv(scaleID,1,GL_FALSE,scaleMat.data());


	d_labelFont =  std::make_shared<GLFont>("Nimbus Mono,Bold",l,512);


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

	d_frameVBO->Render(GL_TRIANGLES);
}

std::string FormatTagID(uint32_t tagID) {
	std::ostringstream oss;
	oss << "0x" << std::setfill('0') << std::setw(3) << std::hex << tagID;
	return oss.str();
}


void GLUserInterface::UploadMatrix(GLuint programID,
                                   const std::string & name,
                                   const Eigen::Matrix3f & matrix) {
	auto matID = glGetUniformLocation(programID,name.c_str());
	glUniformMatrix3fv(matID,1,GL_FALSE,matrix.data());
}

void GLUserInterface::UploadColor(GLuint programID,
                                  const std::string & name,
                                  const cv::Vec3f & color) {
	auto colorID = glGetUniformLocation(programID,name.c_str());
	glUniform3fv(colorID,1,&color[0]);
}

void GLUserInterface::UploadColor(GLuint programID,
                                  const std::string & name,
                                  const cv::Vec4f & color) {
	auto colorID = glGetUniformLocation(programID,name.c_str());
	glUniform4fv(colorID,1,&color[0]);
}


void GLUserInterface::UploadPoints(DrawBuffer & buffer) {
	if ( !buffer.Frame.Message ) {
		return;
	}
	float factor = 1.0 / WindowScaleFactor(buffer);
	std::vector<GLFont::PositionedText> tagLabels;
	GLVertexBufferObject::Matrix points(std::max(buffer.Data.HighlightedIndexes.size(),buffer.Data.NormalIndexes.size()),2);
	tagLabels.reserve(buffer.Data.HighlightedIndexes.size() + buffer.Data.NormalIndexes.size());
	size_t i = -1;
	for ( const auto hIndex : buffer.Data.HighlightedIndexes ) {
		const auto & t = buffer.Frame.Message->tags(hIndex);
		points.block<1,2>(++i,0) = Eigen::Vector2f(t.x(),t.y());
		tagLabels.push_back(std::make_pair(FormatTagID(t.id()),cv::Point(t.x(),t.y())));
	}

	buffer.HighlightedTags->Upload(points.block(0,0,i+1,2),2,0,0);


	i = -1;
	for ( const auto nIndex : buffer.Data.NormalIndexes ) {
		const auto & t = buffer.Frame.Message->tags(nIndex);
		points.block<1,2>(++i,0) = Eigen::Vector2f(t.x(),t.y());
		tagLabels.push_back(std::make_pair(FormatTagID(t.id()),cv::Point(t.x(),t.y())));
	}
	buffer.NormalTags->Upload(points.block(0,0,i+1,2),2,0,0);

	d_labelFont->UploadTexts(*buffer.TagLabels,
	                         factor,
	                         tagLabels.cbegin(),
	                         tagLabels.cend());
}

float GLUserInterface::WindowScaleFactor(const DrawBuffer & buffer) const {
	if ( buffer.TrackingSize == cv::Size(0,0) ) {
		throw std::logic_error("No tracking size");
	}
	return std::min(float(d_windowSize.width)/float(buffer.TrackingSize.width),
	                float(d_windowSize.height)/float(buffer.TrackingSize.height));
}

void GLUserInterface::DrawPoints(const DrawBuffer & buffer) {
	if ( !buffer.Frame.Message ) {
		return;
	}
	glUseProgram(d_pointProgram);
	auto factor = WindowScaleFactor(buffer);
	Eigen::Matrix3f scaleMat;
	scaleMat <<  2.0f / float(buffer.Frame.Message->width()),0,-1.0f,
		0, -2.0f / float(buffer.Frame.Message->height()),1.0f,
		0.0f,0.0f,0.0f;
	UploadMatrix(d_pointProgram,"scaleMat",scaleMat);

	UploadColor(d_pointProgram,"circleColor",cv::Vec3f(0.0f,1.0f,1.0f));
	glPointSize(float(HIGHLIGHTED_POINT_SIZE) * factor );
	buffer.HighlightedTags->Render(GL_POINTS);

	glPointSize(float(NORMAL_POINT_SIZE) * factor);
	UploadColor(d_pointProgram,"circleColor",cv::Vec3f(1.0f,0.0f,0.0f));
	buffer.NormalTags->Render(GL_POINTS);
}

void GLUserInterface::DrawLabels(const DrawBuffer & buffer ) {
	RenderText(*buffer.TagLabels,
	           *d_labelFont,
	           cv::Rect(cv::Point(NORMAL_POINT_SIZE * 1.2,
	                              NORMAL_POINT_SIZE * 1.2 +float(LABEL_FONT_SIZE) / WindowScaleFactor(buffer)),
	                    buffer.TrackingSize),
	           cv::Vec4f(1.0f,1.0f,1.0f,1.0f),
	           cv::Vec4f(0.0f,0.0f,0.0f,1.0f));
}

template <typename T>
std::string printLine(const std::string & label, size_t size,const T & value) {
	std::ostringstream oss;
	oss << label << ":"
	    << std::setw(size - label.size() -1)
	    << std::setfill(' ')
	    << std::right
	    << std::fixed
	    << std::setprecision(2)
	    << value;
	return oss.str();
}

void GLUserInterface::DrawInformations(const DrawBuffer & buffer ) {


	glUseProgram(d_primitiveProgram);

	UploadColor(d_primitiveProgram,"primitiveColor",cv::Vec4f(0.0f,0.1f,0.2f,0.7f));

	Eigen::Matrix3f scaleMat;
	scaleMat <<
		float(2.0) / float(d_viewSize.width), 0.0f                                  , -1.0f,
		0.0f                                , -float(2.0) / float(d_viewSize.height),  1.0f,
		0.0f                                , 0.0f                                  , 0.0f;
	UploadMatrix(d_primitiveProgram,"scaleMat",scaleMat);

	std::ostringstream oss;
	oss << buffer.Frame.FrameDropped
	    << " (" << std::fixed << std::setprecision(2)
	    << 100.0f * (buffer.Frame.FrameDropped / float(buffer.Frame.FrameDropped + buffer.Frame.FrameProcessed) ) << "%)";


	std::vector<GLFont::PositionedText> texts
		= {
		   {printLine(" ",OVERLAY_COLS," ") + " ",
		    cv::Point(d_overlayGlyphSize.width,1*d_overlayGlyphSize.height)},
		   {printLine(" Time",OVERLAY_COLS,buffer.Frame.FrameTime.Round(Duration::Millisecond)) + " ",
		    cv::Point(d_overlayGlyphSize.width,2*d_overlayGlyphSize.height)},
		   {printLine(" Tags",OVERLAY_COLS,buffer.Frame.Message->tags_size()) + " ",
		    cv::Point(d_overlayGlyphSize.width,4*d_overlayGlyphSize.height)},
		   {printLine(" Quads",OVERLAY_COLS,buffer.Frame.Message->quads()) + " ",
		    cv::Point(d_overlayGlyphSize.width,5*d_overlayGlyphSize.height)},
		   {printLine(" FPS",OVERLAY_COLS,buffer.Frame.FPS) + " ",
		    cv::Point(d_overlayGlyphSize.width,7*d_overlayGlyphSize.height)},
		   {printLine(" Frame Processed",OVERLAY_COLS,buffer.Frame.FrameProcessed) + " ",
		    cv::Point(d_overlayGlyphSize.width,8*d_overlayGlyphSize.height)},
		   {printLine(" Frame Dropped",OVERLAY_COLS,oss.str()) + " ",
		    cv::Point(d_overlayGlyphSize.width,9*d_overlayGlyphSize.height)},
		   {printLine(" ",OVERLAY_COLS," ") + " ",
		    cv::Point(d_overlayGlyphSize.width,10*d_overlayGlyphSize.height)},
	};

	auto bBox = d_overlayFont->UploadTexts(*d_textOverlayVBO,
	                                       1.0,
	                                       texts.cbegin(),
	                                       texts.cend());


	GLVertexBufferObject::Matrix boxData(6,2);
	boxData <<
		bBox.x             , bBox.y              ,
		bBox.x + bBox.width, bBox.y              ,
		bBox.x             , bBox.y + bBox.height,
		bBox.x             , bBox.y + bBox.height,
		bBox.x + bBox.width, bBox.y              ,
		bBox.x + bBox.width, bBox.y + bBox.height;
	d_boxOverlayVBO->Upload(boxData,2,0,0);
	d_boxOverlayVBO->Render(GL_TRIANGLES);

	RenderText(*d_textOverlayVBO,
	           *d_overlayFont,
	           cv::Rect(cv::Point(0,0),
	                    d_viewSize),
	           cv::Vec4f(1.0f,1.0f,1.0f,1.0f),
	           cv::Vec4f(1.0f,1.0f,1.0f,0.0f));


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
	DrawLabels(buffer);
	DrawInformations(buffer);

	glfwSwapBuffers(d_window.get());
}


void GLUserInterface::RenderText(const GLVertexBufferObject & buffer,
                                 const GLFont & font,
                                 const cv::Rect & roi,
                                 const cv::Vec4f & foreground,
                                 const cv::Vec4f & background) {
	glUseProgram(d_fontProgram);
	UploadColor(d_fontProgram,"foreground",foreground);
	UploadColor(d_fontProgram,"background",background);
	Eigen::Matrix3f scaleMat;
	scaleMat <<
		2.0f/ float(roi.width), 0.0f                     , -1.0f + float(roi.x) / float(roi.width),
		0.0f                  , -2.0f / float(roi.height), 1.0f - float(roi.y) / float(roi.height),
		0.0f                  , 0.0f                     , 0.0f;
	UploadMatrix(d_fontProgram,"scaleMat",scaleMat);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,font.TextureID());
	glUniform1i(font.TextureID(), 0);

	buffer.Render(GL_TRIANGLES);
}



} // namespace artemis
} // namespace fort

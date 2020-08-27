
#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include "GLUserInterface.hpp"

extern "C" {
#include <ui/shaders.c>
}

#include <glog/logging.h>


#define throw_glfw_error(ctx) do {	  \
		const char * glfwErrorDescription; \
		glfwGetError(&glfwErrorDescription); \
		throw std::runtime_error(std::string(ctx) + ": " + glfwErrorDescription); \
	} while(0)




namespace fort {
namespace artemis {


GLuint CompileShader(const std::string & shaderCode, int type) {
	GLuint shaderID = glCreateShader(type);
	GLint result = GL_FALSE;
	int infoLength;

	const char * code = shaderCode.c_str();
	glShaderSource(shaderID,1,&code,NULL);
	glCompileShader(shaderID);

	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLength);
	if ( infoLength > 0 ){
		std::vector<char> errorMessage(infoLength+1);
		glGetShaderInfoLog(shaderID, infoLength, NULL, &errorMessage[0]);
		throw std::runtime_error("Could not compile shader: " + std::string(errorMessage.begin(),
		                                                                    errorMessage.end()));
	}

	return shaderID;
}

GLuint LoadShaders(const std::string &vertexShaderCode,
                   const std::string &fragmentShaderCode){

	// Create the shaders
	DLOG(INFO) << "[GLUserInterface]: compiling vertex shader";
	GLuint vertexShaderID = CompileShader(vertexShaderCode,GL_VERTEX_SHADER);
	DLOG(INFO) << "[GLUserInterface]: compiling fragment shader";
	GLuint fragmentShaderID = CompileShader(fragmentShaderCode,GL_FRAGMENT_SHADER);


	DLOG(INFO) << "[GLUserInterface]: linking program";
	GLuint programID = glCreateProgram();
	glAttachShader(programID, vertexShaderID);
	glAttachShader(programID, fragmentShaderID);
	glLinkProgram(programID);

	GLint result;
	int infoLogLength;
	// Check the program
	glGetProgramiv(programID, GL_LINK_STATUS, &result);
	glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
	if ( infoLogLength > 0 ){
		std::vector<char> programErrorMessage(infoLogLength+1);
		glGetProgramInfoLog(programID, infoLogLength, NULL, &programErrorMessage[0]);
		throw std::runtime_error("Could not link shader: " + std::string(programErrorMessage.begin(),
			       programErrorMessage.end()));
	}

	glDetachShader(programID, vertexShaderID);
	glDetachShader(programID, fragmentShaderID);

	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return programID;
}


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

void GLUserInterface::InitGLData() {
	DLOG(INFO) << "[GLUserInterface]: InitGLData";
	//InitPBOs();

	ComputeViewport();

	glClearColor(0.0f,0.0f,0.4f,0.0f);

	GLuint VAO;
	glGenVertexArrays(1,&VAO);
	glBindVertexArray(VAO);

	d_frameProgram = LoadShaders(std::string(frame_vertexshader,frame_vertexshader+frame_vertexshader_size),
	                             std::string(frame_fragmentshader,frame_fragmentshader+frame_fragmentshader_size));

	static const GLfloat frameData[] = {
	                                    -1.0f, -1.0f, 0.0f,
	                                    1.0f, -1.0f, 0.0f,
	                                    1.0f,  1.0f, 0.0f,
	                                    //	                                    1.0f,  1.0f, 0.0f,
	                                    // -1.0f,  1.0f, 0.0f,
	                                    // -1.0f,  -1.0f, 0.0f
	};

	glGenBuffers(1,&d_frameVBO);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameVBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(frameData),frameData,GL_STATIC_DRAW);


}

void GLUserInterface::InitPBOs() {
	DLOG(INFO) << "[GLUserInterface]: initialiazing PBOs";

	glGenBuffers(1,&(d_buffer[0].PBO));
	glGenBuffers(1,&(d_buffer[1].PBO));
	for ( size_t i = 0; i < 2; ++i) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER,d_buffer[i].PBO);
		glBufferData(GL_PIXEL_UNPACK_BUFFER,d_workingSize.width*d_workingSize.height,0,GL_STREAM_DRAW);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
}

GLUserInterface::GLFWwindowPtr GLUserInterface::OpenWindow(const cv::Size & size) {
	DLOG(INFO) << "[GLUserInterface]: Opening window";

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	auto window = glfwCreateWindow(size.width/2,
	                               size.height/2,
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

void GLUserInterface::OnSizeChanged(int width,int height) {
	DLOG(INFO) << "New framebuffer " << width << "x" << height;
	d_windowSize = cv::Size(width,height);
	ComputeViewport();
	Draw();
}


void GLUserInterface::Draw() {
	const auto & nextBuffer = d_buffer[(d_index+1)%2];
	const auto & currentBuffer = d_buffer[d_index];

	//	UploadTexture(nextBuffer);

	Draw(currentBuffer);
}

bool operator==(const UserInterface::Zoom & a,
                const UserInterface::Zoom & b) {
	return a.Scale == b.Scale && a.Center == b.Center;
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


void GLUserInterface::ComputeViewport() {
	int wantedWidth = d_workingSize.width * double(d_windowSize.height) / double(d_workingSize.height);
	if ( wantedWidth <= d_windowSize.width ) {
		DLOG(INFO) << "[GLUserInterface]: Viewport full height " << wantedWidth << "x" << d_windowSize.height;

		glViewport((d_windowSize.width - wantedWidth) / 2,
		           0,
		           wantedWidth,
		           d_windowSize.height);
	} else {
		wantedWidth = d_windowSize.width;
		int wantedHeight = d_workingSize.height * double(d_windowSize.width) / double(d_workingSize.width);
		DLOG(INFO) << "[GLUserInterface]: Viewport full width " << d_windowSize.width << "x" << wantedHeight;
		glViewport(0,
		           (d_windowSize.height - wantedHeight) / 2,
		           d_windowSize.width,
		           wantedHeight);
	}
}


void GLUserInterface::Draw(const DrawBuffer & buffer ) {
	if ( !buffer.Frame.Full ) {
		DLOG(WARNING) << "Skipping";
		return;
	}

	if ( d_zoom.Scale != 1.0 && !(buffer.Frame.CurrentZoom == d_zoom) ) {
		// TODO: update UV coordinates
	}
	DLOG(INFO) << "Drawing";
	glClear( GL_COLOR_BUFFER_BIT);

	glUseProgram(d_frameProgram);

	// glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer.PBO);

	//glBindTexture(GL_TEXTURE_2D,d_textureID);
	//glTexImage2D(GL_TEXTURE_2D,0,GL_R8,d_workingSize.width,d_workingSize.height,0,GL_RED,GL_UNSIGNED_BYTE,0);


	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, d_frameVBO);
	glVertexAttribPointer(0,
	                      3,
	                      GL_FLOAT,
	                      GL_FALSE,
	                      0,
	                      (void*)0);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(0);


	glfwSwapBuffers(d_window.get());
}



} // namespace artemis
} // namespace fort

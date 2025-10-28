#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <Eigen/Core>

#include <glog/logging.h>

#include "../Utils.hpp"

#include "GLUserInterface.hpp"

#include "artemis-common_rc.h"
#include "fort/gl/Shader.hpp"

#include <iomanip>

#if (GLFW_VERSION_MAJOR * 100 + GLFW_VERSION_MINOR) < 303
#define IMPLEMENT_GLFW_GET_ERROR 1
#endif

#ifdef IMPLEMENT_GLFW_GET_ERROR
#include <deque>
#include <mutex>
#endif

#define throw_glfw_error(ctx)                                                  \
	do {                                                                       \
		const char *glfwErrorDescription;                                      \
		glfwGetError(&glfwErrorDescription);                                   \
		throw std::runtime_error(                                              \
		    std::string(ctx) + ": " + glfwErrorDescription                     \
		);                                                                     \
	} while (0)

namespace fort {
namespace artemis {

#ifdef IMPLEMENT_GLFW_GET_ERROR

std::deque<std::pair<int, const char *>> GLFWErrorStack;
std::mutex                               GLFWErrorMutex;

void GLFWErrorCallback(int error, const char *description) {
	std::lock_guard<std::mutex> lock(GLFWErrorMutex);
	GLFWErrorStack.push_back({error, description});
}
}
}

int glfwGetError(const char **description) {
	std::lock_guard<std::mutex> lock(fort::artemis::GLFWErrorMutex);
	if (fort::artemis::GLFWErrorStack.empty()) {
		return 0;
	}
	auto res = fort::artemis::GLFWErrorStack.front();
	if (description != nullptr) {
		*description = res.second;
	}
	fort::artemis::GLFWErrorStack.pop_front();
	return res.first;
}

namespace fort {
namespace artemis {

#endif // IMPLEMENT_GLFW_GET_ERROR

GLUserInterface::GLUserInterface(
    const cv::Size      &workingResolution,
    const cv::Size      &fullSize,
    const Options       &options,
    const ROIChannelPtr &roiChannel
)
    : UserInterface(workingResolution, options, roiChannel)
    , fort::gl::
          Window{workingResolution.width, workingResolution.height, "artemis"}
    , d_index(0)
    , d_workingSize(workingResolution)
    , d_fullSize(fullSize)
    , d_windowSize(workingResolution)
    , d_currentScaleFactor(0)
    , d_currentPOI(fullSize.width / 2, fullSize.height / 2)
    , d_ROISize(options.NewAntROISize) {

#ifdef IMPLEMENT_GLFW_GET_ERROR
	glfwSetErrorCallback(&GLFWErrorCallback);
#endif // IMPLEMENT_GLFW_GET_ERROR
	DLOG(INFO) << "[GLUserInterface]: initialiazing GLFW";

	if (!glfwInit()) {
		throw_glfw_error("could not initilaize GLFW");
	}
}

void GLUserInterface::OnKey(int key, int scancode, int action, int mods) {
	if (PromptAndValue().empty() == false) {
		if (action == GLFW_PRESS && mods == 0 && key == GLFW_KEY_ENTER) {
			LeaveHighlightPrompt();
			Draw();
		}
		return;
	}

	if (action != GLFW_PRESS) {
		return;
	}

	static std::map<std::pair<int, int>, std::function<void()>> actions = {
	    {{GLFW_KEY_UP, 0}, [this]() { Displace({0.0f, 0.2f}); }},
	    {{GLFW_KEY_DOWN, 0}, [this]() { Displace({0.0f, -0.2f}); }},
	    {{GLFW_KEY_LEFT, 0}, [this]() { Displace({-0.2f, 0.0f}); }},
	    {{GLFW_KEY_RIGHT, 0}, [this]() { Displace({0.2f, 0.0f}); }},
	    {{GLFW_KEY_I, 0}, [this]() { Zoom(+1); }},
	    {{GLFW_KEY_O, 0}, [this]() { Zoom(-1); }},
	    {{GLFW_KEY_I, GLFW_MOD_SHIFT}, [this]() { Zoom(2000); }},
	    {{GLFW_KEY_O, GLFW_MOD_SHIFT},
	     [this]() { Zoom(-this->d_currentScaleFactor); }},
	    {{GLFW_KEY_H, 0},
	     [this]() {
		     ToggleDisplayHelp();
		     Draw();
	     }},
	    {{GLFW_KEY_R, 0},
	     [this]() {
		     ToggleDisplayROI();
		     Draw();
	     }},
	    {{GLFW_KEY_D, 0},
	     [this]() {
		     ToggleDisplayOverlay();
		     Draw();
	     }},
	    {{GLFW_KEY_L, 0},
	     [this]() {
		     ToggleDisplayLabels();
		     Draw();
	     }},
	    {{GLFW_KEY_T, 0},
	     [this]() {
		     EnterHighlightPrompt();
		     Draw();
	     }},

	};
	auto fi = actions.find({key, mods});
	if (fi == actions.end()) {
		return;
	}
	fi->second();
}

void GLUserInterface::Zoom(int increment) {
	const static float scaleIncrement = 0.5; // increments by 25%
	int                maxScaleFactor =
	    std::ceil((float(d_fullSize.height) / 400.0f - 1.0f) / scaleIncrement);
	d_currentScaleFactor =
	    std::min(std::max(d_currentScaleFactor + increment, 0), maxScaleFactor);
	float    scale = 1.0 / (1.0f + scaleIncrement * d_currentScaleFactor);
	cv::Size wantedSize =
	    cv::Size(d_fullSize.width * scale, d_fullSize.height * scale);

	UpdateROI(GetROICenteredAt(d_currentPOI, wantedSize, d_fullSize));
}

void GLUserInterface::Displace(const Eigen::Vector2f &offset) {
	d_currentPOI.x += offset.x() / d_roiProjection(0, 0);
	d_currentPOI.y += offset.y() / d_roiProjection(1, 1);
	UpdateROI(GetROICenteredAt(d_currentPOI, d_ROI.size(), d_fullSize));
}

void GLUserInterface::UpdateROI(const cv::Rect &ROI) {
	auto &buffer = d_buffer[d_index];
	d_ROI        = ROI;
	d_currentPOI =
	    cv::Point(d_ROI.x + +d_ROI.width / 2, d_ROI.y + d_ROI.height / 2);
	ComputeProjection(d_ROI, d_roiProjection);
	if (buffer.FullUploaded == false) {
		UploadTexture(buffer);
	}
	Draw();
	ROIChanged(d_ROI);
}

void GLUserInterface::OnText(unsigned int codepoint) {
	if (PromptAndValue().empty() || (Value().empty() && codepoint == 't')) {
		return;
	}

	if (codepoint < 255) {
		AppendPromptValue(codepoint);
		Draw();
	}
}

void GLUserInterface::OnMouseMove(double x, double y) {}

void GLUserInterface::OnMouseInput(int button, int action, int mods) {}

void GLUserInterface::OnScroll(double xOffset, double yOffset) {}

GLUserInterface::~GLUserInterface() {}

void GLUserInterface::Task() {
	Process();
}

void GLUserInterface::UpdateFrame(
    const FrameToDisplay &frame, const DataToDisplay &data
) {

	auto &buffer = d_buffer[d_index];
	buffer.Frame = frame;
	buffer.Data  = data;
	if (!frame.Message) {
		buffer.TrackingSize = cv::Size(0, 0);
	} else {
		buffer.TrackingSize =
		    cv::Size(frame.Message->width(), frame.Message->height());
	}
	UploadTexture(buffer);
	UploadPoints(buffer);
	d_index = (d_index + 1) % 2;

	Update();
}

void GLUserInterface::ComputeViewport() {
	float windowRatio =
	    float(d_windowSize.height) / float(d_workingSize.height);
	int wantedWidth = d_workingSize.width * windowRatio;
	if (wantedWidth <= d_windowSize.width) {
		DLOG(INFO) << "[GLUserInterface]: Viewport with full height "
		           << wantedWidth << "x" << d_windowSize.height;

		glViewport(
		    (d_windowSize.width - wantedWidth) / 2,
		    0,
		    wantedWidth,
		    d_windowSize.height
		);
		d_viewSize = cv::Size(wantedWidth, d_windowSize.height);
	} else {
		wantedWidth = d_windowSize.width;
		windowRatio = float(d_windowSize.width) / float(d_workingSize.width);
		int wantedHeight = d_workingSize.height * windowRatio;
		DLOG(INFO) << "[GLUserInterface]: Viewport with full width "
		           << d_windowSize.width << "x" << wantedHeight;
		glViewport(
		    0,
		    (d_windowSize.height - wantedHeight) / 2,
		    d_windowSize.width,
		    wantedHeight
		);
		d_viewSize = cv::Size(d_windowSize.width, wantedHeight);
	}
}

void GLUserInterface::OnSizeChanged(int width, int height) {
	DLOG(INFO) << "New framebuffer size " << width << "x" << height;

	d_windowSize = cv::Size(width, height);
	ComputeViewport();
	ComputeProjection(cv::Rect(cv::Point(0, 0), d_viewSize), d_viewProjection);
	Draw();
}

void GLUserInterface::Draw() {
	const auto &currentBuffer = d_buffer[d_index];
	Draw(currentBuffer);
}

void GLUserInterface::UploadTexture(DrawBuffer &buffer) {
	std::shared_ptr<cv::Mat> toUpload = buffer.Frame.Full;
	buffer.FullUploaded               = true;
	if (buffer.Frame.Zoomed && d_ROI == buffer.Frame.CurrentROI) {
		toUpload            = buffer.Frame.Zoomed;
		buffer.FullUploaded = false;
	}

	if (!toUpload) {
		return;
	}

	size_t frameSize = toUpload->cols * toUpload->rows;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, frameSize, 0, GL_STREAM_DRAW);
	auto ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	if (ptr != nullptr) {
		memcpy(ptr, toUpload->data, frameSize);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLUserInterface::InitGLData() {
	DLOG(INFO) << "[GLUserInterface]: InitGLData";

	ComputeViewport();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	d_frameProgram = fort::gl::CompileProgram(
	    std::string(
	        ui_shaders_frame_vertexshader_start,
	        ui_shaders_frame_vertexshader_end
	    ),
	    std::string(
	        ui_shaders_frame_fragmentshader_start,
	        ui_shaders_frame_fragmentshader_end
	    )
	);

	glGenTextures(1, &d_frameTexture);
	glBindTexture(GL_TEXTURE_2D, d_frameTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	d_pointProgram = fort::gl::CompileProgram(
	    std::string(
	        ui_shaders_primitive_vertexshader_start,
	        ui_shaders_primitive_vertexshader_end
	    ),
	    std::string(
	        ui_shaders_circle_fragmentshader_start,
	        ui_shaders_circle_fragmentshader_end
	    )
	);

	d_primitiveProgram = fort::gl::CompileProgram(
	    std::string(
	        ui_shaders_primitive_vertexshader_start,
	        ui_shaders_primitive_vertexshader_end
	    ),
	    std::string(
	        ui_shaders_primitive_fragmentshader_start,
	        ui_shaders_primitive_fragmentshader_end
	    )
	);

	d_fontProgram = fort::gl::CompileProgram(
	    std::string(
	        ui_shaders_font_vertexshader_start,
	        ui_shaders_font_vertexshader_end
	    ),
	    std::string(
	        ui_shaders_font_fragmentshader_start,
	        ui_shaders_font_fragmentshader_end
	    )
	);

	d_roiProgram = fort::gl::CompileProgram(
	    std::string(
	        ui_shaders_primitive_vertexshader_start,
	        ui_shaders_primitive_vertexshader_end
	    ),
	    std::string(
	        ui_shaders_roi_fragmentshader_start,
	        ui_shaders_roi_fragmentshader_end
	    )
	);
	size_t l = LABEL_FONT_SIZE;
	size_t o = OVERLAY_FONT_SIZE;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	d_ROI = cv::Rect(cv::Point(0, 0), d_fullSize);

	ComputeProjection(cv::Rect(cv::Point(0, 0), d_fullSize), d_fullProjection);
	ComputeProjection(d_ROI, d_roiProjection);
	ComputeProjection(cv::Rect(cv::Point(0, 0), d_viewSize), d_viewProjection);
}

void GLUserInterface::ComputeProjection(
    const cv::Rect &roi, Eigen::Matrix3f &res
) {
	res << 2.0f / float(roi.width), 0.0f,
	    -1.0f - 2.0f * float(roi.x) / float(roi.width), 0.0f,
	    -2.0f / float(roi.height),
	    1.0f + 2.0f * float(roi.y) / float(roi.height), 0.0f, 0.0f, 1.0f;
}

void GLUserInterface::DrawMovieFrame(const DrawBuffer &buffer) {
	glUseProgram(d_frameProgram);

	if (d_ROI == buffer.Frame.CurrentROI) {
		// we use an higher resolution zoomed frame, we therefore use the full
		// ROI projection
		fort::gl::Upload(d_frameProgram, "scaleMat", d_fullProjection);
	} else {
		fort::gl::Upload(d_frameProgram, "scaleMat", d_roiProjection);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, d_frameTexture);
	//	glUniform1i(d_frameTexture, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.PBO);
	glTexImage2D(
	    GL_TEXTURE_2D,
	    0,
	    GL_R8,
	    d_workingSize.width,
	    d_workingSize.height,
	    0,
	    GL_RED,
	    GL_UNSIGNED_BYTE,
	    0
	);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

std::string FormatTagID(uint32_t tagID) {
	std::ostringstream oss;
	oss << "0x" << std::setfill('0') << std::setw(3) << std::hex << tagID;
	return oss.str();
}

void GLUserInterface::UploadPoints(DrawBuffer &buffer) {
	if (!buffer.Frame.Message) {
		return;
	}
	float factor = 1.0 / FullToWindowScaleFactor();
}

float GLUserInterface::FullToWindowScaleFactor() const {
	return std::min(
	    float(d_windowSize.width) / float(d_fullSize.width),
	    float(d_windowSize.height) / float(d_fullSize.height)
	);
}

void GLUserInterface::DrawROI(const DrawBuffer &buffer) {
	if (!buffer.Frame.Message || DisplayROI() == false) {
		return;
	}
	glUseProgram(d_roiProgram);
	auto factor = FullToWindowScaleFactor();
	if (d_ROI.size() != d_fullSize) {
		factor *= float(d_fullSize.width) / float(d_ROI.width);
	}
	auto floatPointSize = float(d_ROISize) * factor;

	fort::gl::Upload(d_roiProgram, "scaleMat", d_roiProjection);

	fort::gl::Upload(
	    d_roiProgram,
	    "boxColor",
	    Eigen::Vector3f{0.0f, 1.0f, 1.0f}
	);
	glPointSize(floatPointSize);
	fort::gl::Upload(d_roiProgram, "lineWidth", 3.0f * 2.0f / floatPointSize);
	fort::gl::Upload(
	    d_roiProgram,
	    "boxColor",
	    Eigen::Vector3f{1.0f, 0.0f, 0.0f}
	);
}

void GLUserInterface::DrawPoints(const DrawBuffer &buffer) {
	if (!buffer.Frame.Message) {
		return;
	}
	glUseProgram(d_pointProgram);
	auto factor = FullToWindowScaleFactor();
	if (d_ROI.size() != d_fullSize) {
		factor *= float(d_fullSize.width) / float(d_ROI.width);
	}
	fort::gl::Upload(d_pointProgram, "scaleMat", d_roiProjection);

	fort::gl::Upload(
	    d_pointProgram,
	    "circleColor",
	    Eigen::Vector3f{0.0f, 1.0f, 1.0f}
	);
	glPointSize(float(HIGHLIGHTED_POINT_SIZE) * factor);

	glPointSize(float(NORMAL_POINT_SIZE) * factor);
	fort::gl::Upload(
	    d_pointProgram,
	    "circleColor",
	    Eigen::Vector3f{1.0f, 0.0f, 0.0f}
	);
}

void GLUserInterface::DrawLabels(const DrawBuffer &buffer) {
	if (DisplayLabels() == false) {
		return;
	}
	auto labelROI = cv::Rect(
	    cv::Point(
	        d_ROI.x - NORMAL_POINT_SIZE * 0.7,
	        d_ROI.y - NORMAL_POINT_SIZE * 0.7
	    ),
	    d_ROI.size()
	);
}

template <typename T>
std::string printLine(const std::string &label, size_t size, const T &value) {
	std::ostringstream oss;
	oss << " " << label << ":" << std::setw(size - label.size() - 1)
	    << std::setfill(' ') << std::right << std::fixed << std::setprecision(2)
	    << value << " ";
	return oss.str();
}

void GLUserInterface::DrawInformations(const DrawBuffer &buffer) {
	if (DisplayOverlay() == false) {
		return;
	}

	glUseProgram(d_primitiveProgram);

	fort::gl::Upload(d_primitiveProgram, "primitiveColor", OVERLAY_BACKGROUND);

	fort::gl::Upload(d_primitiveProgram, "scaleMat", d_viewProjection);

	std::ostringstream dropOss;
	dropOss
	    << buffer.Frame.FrameDropped << " (" << std::fixed
	    << std::setprecision(2)
	    << 100.0f *
	           (buffer.Frame.FrameDropped /
	            float(buffer.Frame.FrameDropped + buffer.Frame.FrameProcessed))
	    << "%)";
	std::ostringstream oss;
	std::ostringstream videoDropOss;
	if (buffer.Frame.VideoOutputProcessed == -1) {
		videoDropOss << "No Video Output";
	} else {
		// we may have not yet processed or dropped any frame
		size_t totalVideo = std::max(
		    1LU,
		    buffer.Frame.VideoOutputProcessed + buffer.Frame.VideoOutputDropped
		);
		videoDropOss << buffer.Frame.VideoOutputDropped << " (" << std::fixed
		             << std::setprecision(2)
		             << 100.0f * float(buffer.Frame.VideoOutputDropped) /
		                    float(totalVideo)
		             << "%)";
	}

	oss << std::string(OVERLAY_COLS + 2, '|') << std::endl
	    << printLine(
	           "Time",
	           OVERLAY_COLS,
	           buffer.Frame.FrameTime.Round(Duration::Millisecond)
	       )
	    << std::endl
	    << std::endl
	    << printLine("Tags", OVERLAY_COLS, buffer.Frame.Message->tags_size())
	    << std::endl
	    << printLine("Quads", OVERLAY_COLS, buffer.Frame.Message->quads())
	    << std::endl
	    << std::endl
	    << printLine("FPS", OVERLAY_COLS, buffer.Frame.FPS) << std::endl
	    << printLine(
	           "Frame Processed",
	           OVERLAY_COLS,
	           buffer.Frame.FrameProcessed
	       )
	    << std::endl
	    << printLine("Frame Dropped", OVERLAY_COLS, dropOss.str()) << std::endl
	    << printLine("Video Dropped", OVERLAY_COLS, videoDropOss.str())
	    << std::endl
	    << std::string(OVERLAY_COLS + 2, '|');
}

void GLUserInterface::Draw(const DrawBuffer &buffer) {
	if (!buffer.Frame.Full) {
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT);

	DrawMovieFrame(buffer);
	DrawPoints(buffer);
	DrawROI(buffer);
	DrawLabels(buffer);
	DrawWatermark();
	DrawInformations(buffer);
	DrawHelp();
	DrawPrompt();
}

void GLUserInterface::DrawWatermark() {}

template <typename T> std::string Centered(const T &t, size_t nChars) {
	std::ostringstream oss;
	oss << t;
	if (oss.str().size() >= nChars) {
		return oss.str();
	}
	int left = (nChars - oss.str().size()) / 2;
	return std::string(left, ' ') + oss.str() +
	       std::string(' ', nChars - oss.str().size() - left);
}

void GLUserInterface::DrawHelp() {
	if (DisplayHelp() == false) {
		return;
	}

	const static size_t COLS = 50;
	std::ostringstream  help;
	help << std::string(COLS + 2, '|') << std::endl
	     << Centered("-- Artemis User Interface Commands --", COLS + 2)
	     << std::endl
	     << std::endl
	     << printLine("<T>", COLS, "Enters prompt to modify tag highlights")
	     << std::endl
	     << printLine("<R>", COLS, "Toggles displays of new ant ROI")
	     << std::endl
	     << printLine("<L>", COLS, "Toggles displays of tag labels")
	     << std::endl
	     << printLine("<D>", COLS, "Toggles displays of data overlay")
	     << std::endl
	     << printLine("<I>", COLS, "Zooms In") << std::endl
	     << printLine("<O>", COLS, "Zooms Out") << std::endl
	     << printLine("<Shift+I>", COLS, "Maximum Zoom IN") << std::endl
	     << printLine("<Shift+O>", COLS, "Zooms Out") << std::endl
	     << std::string(COLS + 2, '|');
}

void GLUserInterface::DrawPrompt() {
	if (PromptAndValue().empty() == true) {
		return;
	}
	glUseProgram(d_primitiveProgram);
	fort::gl::Upload(d_primitiveProgram, "scaleMat", d_viewProjection);
	fort::gl::Upload(d_primitiveProgram, "primitiveColor", OVERLAY_BACKGROUND);
}

const Eigen::Vector4f GLUserInterface::OVERLAY_GLYPH_FOREGROUND = {
    1.0f, 1.0f, 1.0f, 1.0f
};
const Eigen::Vector4f GLUserInterface::OVERLAY_GLYPH_BACKGROUND = {
    1.0f, 1.0f, 1.0f, 0.0f
};
const Eigen::Vector4f GLUserInterface::OVERLAY_BACKGROUND = {
    0.05, 0.1f, 0.2f, 0.7f
};
const Eigen::Vector4f GLUserInterface::LABEL_FOREGROUND = {
    1.0f, 1.0f, 1.0f, 1.0f
};
const Eigen::Vector4f GLUserInterface::LABEL_BACKGROUND = {
    0.0f, 0.0f, 0.0f, 1.0f
};

} // namespace artemis
} // namespace fort

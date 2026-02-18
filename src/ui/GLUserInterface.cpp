#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <Eigen/Core>

#include "GLUserInterface.hpp"

#include "Rect.hpp"
#include "artemis-common_rc.h"
#include "fort/gl/Shader.hpp"
#include "fort/gl/TextRenderer.hpp"
#include "fort/gl/VAOPool.hpp"
#include "utils/Slog.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>

#include <slog++/Attribute.hpp>
#include <slog++/Level.hpp>
#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {

slog::Attribute slogSize(const char *name, const Size &s) {
	return slog::Group(
	    name,
	    slog::Int("width", s.width()),
	    slog::Int("height", s.height())
	);
};

std::string FormatTagID(uint32_t tagID) {
	std::ostringstream oss;
	oss << "0x" << std::setfill('0') << std::setw(3) << std::hex << tagID;
	return oss.str();
}

GLUserInterface::GLUserInterface(
    const Size          &workingResolution,
    const Size          &fullSize,
    const Options       &options,
    const ROIChannelPtr &roiChannel
)
    : UserInterface(workingResolution, options, roiChannel)
    , fort::gl::
          Window{workingResolution.width(), workingResolution.height(), "artemis"}
    , d_index(0)
    , d_workingSize{workingResolution}
    , d_inputSize(fullSize)
    , d_inputToWorkingRatio{float(fullSize.width()) / float(workingResolution.width())}
    , d_individualROISize{options.CloseUpROISize}
    , d_windowSize{workingResolution}
    , d_currentScaleFactor{0}
    , d_currentPOI{fullSize.width() / 2, fullSize.height() / 2}
    , d_logger{slog::With(slog::String("group", "GLUserInterface"))}
    , d_labelFont{"Nimbus Mono,Bold", LABEL_FONT_SIZE}
    , d_overlayFont{"Ubuntu Mono", OVERLAY_FONT_SIZE}
    , d_watermarkFont{"Ubuntu Mono", WATERMARK_FONT_SIZE}
    , d_labelCache{[this](uint32_t tagID) {
	    return std::make_shared<gl::CompiledText>(
	        std::move(d_labelFont.Compile(FormatTagID(tagID), 2))
	    );
    }} {

	InitGLData();
}

void GLUserInterface::OnKey(int key, int scancode, int action, int mods) {
	d_logger.DDebug(
	    "OnKey",
	    slog::Int("key", key),
	    slog::Int("scancode", scancode),
	    slog::Int("action", action),
	    slog::Int("mods", mods)
	);
	if (PromptAndValue().empty() == false) {
		if (action != GLFW_PRESS || mods != 0) {
			return;
		}
		switch (key) {
		case GLFW_KEY_ENTER:
			LeaveHighlightPrompt();
			break;
		case GLFW_KEY_DELETE:
		case GLFW_KEY_BACKSPACE:
			ErasePromptLastValue();
			d_promptText = d_overlayFont.Compile(PromptAndValue());
			break;
		default:
			return;
		}
		Update();
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
		     Update();
	     }},
	    {{GLFW_KEY_R, 0},
	     [this]() {
		     ToggleDisplayROI();
		     Update();
	     }},
	    {{GLFW_KEY_D, 0},
	     [this]() {
		     ToggleDisplayOverlay();
		     Update();
	     }},
	    {{GLFW_KEY_L, 0},
	     [this]() {
		     ToggleDisplayLabels();
		     Update();
	     }},
	    {{GLFW_KEY_T, 0},
	     [this]() {
		     EnterHighlightPrompt();
		     d_promptText = d_overlayFont.Compile(PromptAndValue());
		     Update();
	     }},

	};
	auto fi = actions.find({key, mods});
	if (fi == actions.end()) {
		return;
	}
	fi->second();
}

void GLUserInterface::Zoom(int increment) {
	constexpr static float scaleIncrement = 0.5; // increments by 25%

	int maxScaleFactor = std::ceil(
	    (float(d_inputSize.height()) / 400.0f - 1.0f) / scaleIncrement
	);

	d_currentScaleFactor =
	    std::clamp(d_currentScaleFactor + increment, 0, maxScaleFactor);

	float scale = 1.0 / (1.0f + scaleIncrement * d_currentScaleFactor);
	Eigen::Vector2i wantedSize{
	    d_inputSize.width() * scale,
	    d_inputSize.height() * scale
	};

	UpdateROI(GetROICenteredAt(d_currentPOI, wantedSize, d_inputSize));
}

void GLUserInterface::Displace(const Eigen::Vector2f &offset) {
	d_currentPOI.x() += offset.x() / d_roiProjection(0, 0);
	d_currentPOI.y() += offset.y() / d_roiProjection(1, 1);
	UpdateROI(GetROICenteredAt(d_currentPOI, d_ROI.Size(), d_inputSize));
}

void GLUserInterface::UpdateROI(const Rect &ROI) {
	d_ROI        = ROI;
	d_currentPOI = {
	    d_ROI.x() + d_ROI.width() / 2,
	    d_ROI.y() + d_ROI.height() / 2
	};
	ComputeProjection(d_ROI, d_roiProjection);
	// If the full details is not uploaded, upload it to both current and next
	// buffer otherwise some weird zoom in - zoom out will appear.
	for (auto &b : d_buffers) {
		if (b.FullUploaded == false) {
			UploadTexture(b);
		}
	}

	ROIChanged(d_ROI);
	Update();
}

void GLUserInterface::OnText(unsigned int codepoint) {
	d_logger.DInfo("OnText", slog::Int("codepoint", codepoint));
	if (PromptAndValue().empty() || (Value().empty() && codepoint == 't')) {
		return;
	}

	if (codepoint < 255) {
		AppendPromptValue(codepoint);
		d_promptText = d_overlayFont.Compile(PromptAndValue());
		Update();
	}
}

void GLUserInterface::OnMouseMove(double x, double y) {
	d_mousePosition = {x, y};
	if (d_mouseDrag.has_value() == false) {
		return;
	}

	auto delta = (d_mousePosition - d_mouseDrag.value().Mouse);

	d_currentPOI = d_mouseDrag.value().POI - delta.cast<int>();
	UpdateROI(GetROICenteredAt(d_currentPOI, d_ROI.Size(), d_inputSize));
}

void GLUserInterface::OnMouseInput(int button, int action, int mods) {
	d_logger.DDebug(
	    "OnMouseInput",
	    slog::Int("button", button),
	    slog::Int("action", action),
	    slog::Int("mods", mods)
	);
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1 &&
	    d_mouseDrag.has_value() == false) {
		d_mouseDrag = {.Mouse = d_mousePosition, .POI = d_currentPOI};
		return;
	}

	if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_1 &&
	    d_mouseDrag.has_value() == true) {
		d_mouseDrag = std::nullopt;
		return;
	}
}

void GLUserInterface::OnScroll(double xOffset, double yOffset) {
	d_logger.DDebug(
	    "OnScroll",
	    slog::Float("xOffset", xOffset),
	    slog::Float("yOffset", yOffset)
	);
	if (yOffset > 0) {
		Zoom(1);
	} else if (yOffset < 0) {
		Zoom(-1);
	}
}

GLUserInterface::~GLUserInterface() = default;

void GLUserInterface::Task() {
	Process();
}

void GLUserInterface::UpdateFrame(
    const FrameToDisplay &frame, const DataToDisplay &data
) {

	auto &buffer = d_buffers[d_index];
	buffer.Frame = frame;
	buffer.Data  = data;
	if (!frame.Message) {
		buffer.TrackingSize = Size{0, 0};
	} else {
		buffer.TrackingSize =
		    Size{frame.Message->width(), frame.Message->height()};
	}
	UploadTexture(buffer);
	UploadPoints(buffer);
	UploadInformations(buffer);

	d_index = (d_index + 1) % 2;

	Update();
}

void GLUserInterface::ComputeViewport() {
	auto logger = d_logger.With(
	    slogSize("window", d_windowSize),
	    slogSize("frame", d_workingSize)
	);
	float windowRatio =
	    float(d_windowSize.height()) / float(d_workingSize.height());
	int wantedWidth = d_workingSize.width() * windowRatio;
	if (wantedWidth <= d_windowSize.width()) {
		logger.DInfo(
		    "Viewport with full height",
		    slog::Int("width", wantedWidth),
		    slog::Int("height", d_windowSize.height()),
		    slog::Int("x", (d_windowSize.width() - wantedWidth) / 2),
		    slog::Int("y", 0)
		);

		glViewport(
		    (d_windowSize.width() - wantedWidth) / 2,
		    0,
		    wantedWidth,
		    d_windowSize.height()
		);
		d_viewSize = {wantedWidth, d_windowSize.height()};
	} else {
		wantedWidth = d_windowSize.width();
		windowRatio =
		    float(d_windowSize.width()) / float(d_workingSize.width());
		int wantedHeight = d_workingSize.height() * windowRatio;
		logger.DInfo(
		    "Viewport with full width",
		    slog::Int("width", d_windowSize.width()),
		    slog::Int("height", wantedHeight),
		    slog::Int("x", 0),
		    slog::Int("y", (d_windowSize.height() - wantedHeight) / 2)
		);
		glViewport(
		    0,
		    (d_windowSize.height() - wantedHeight) / 2,
		    d_windowSize.width(),
		    wantedHeight
		);
		d_viewSize = {d_windowSize.width(), wantedHeight};
	}
}

void GLUserInterface::OnSizeChanged(int width, int height) {
	d_logger.DInfo(
	    "New framebuffer size",
	    slog::Int("width", width),
	    slog::Int("height", height)
	);

	d_windowSize = {width, height};
	ComputeViewport();
	ComputeProjection(Rect{{0, 0}, d_viewSize}, d_viewProjection);
	Update();
}

void GLUserInterface::Draw() {
	const auto &currentBuffer = d_buffers[d_index];
	Draw(currentBuffer);
}

void GLUserInterface::UploadTexture(DrawBuffer &buffer) {
	std::shared_ptr<ImageU8> toUpload = buffer.Frame.Full;
	buffer.FullUploaded               = true;
	if (buffer.Frame.Zoomed && d_ROI == buffer.Frame.CurrentROI) {
		slog::DDebug(
		    "Uploading zoomed",
		    slog::Int("FrameID", buffer.Frame.FrameID),
		    slogRect("ROI", d_ROI),
		    slogRect("FrameROI", buffer.Frame.CurrentROI)
		);
		toUpload            = buffer.Frame.Zoomed;
		buffer.FullUploaded = false;
	} else {
		slog::DDebug(
		    "Uploading full",
		    slog::Int("FrameID", buffer.Frame.FrameID),
		    slogRect("ROI", d_ROI),
		    slogRect("FrameROI", buffer.Frame.CurrentROI)
		);
	}

	if (!toUpload) {
		return;
	}

	size_t frameSize = toUpload->height * toUpload->stride;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, frameSize, 0, GL_STREAM_DRAW);
	auto ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	if (ptr != nullptr) {
		memcpy(ptr, toUpload->buffer, frameSize);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLUserInterface::InitGLData() {
	d_logger.DInfo("InitGLData");

	InitPBOs();

	ComputeViewport();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	d_frameBuffer = std::make_unique<fort::gl::VertexArrayObject>();
	// clang-format off
	float frameData[24] = {
	    0.0f                     , 0.0f                      , 0.0f, 0.0f, //
	    float(d_inputSize.width()), 0.0f                      , 1.0f, 0.0f, //
	    float(d_inputSize.width()), float(d_inputSize.height()), 1.0f, 1.0f, //
	    float(d_inputSize.width()), float(d_inputSize.height()), 1.0f, 1.0f, //
	    0.0f                     , float(d_inputSize.height()), 0.0f, 1.0f, //
	    0.0f                     , 0.0f                      , 0.0f, 0.0f, //
	};
	// clang-format on
	d_frameBuffer->BufferData<float, 2, 2>(GL_STATIC_DRAW, frameData, 24);

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

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	d_ROI = Rect({0, 0}, d_inputSize);

	ComputeProjection(Rect({0, 0}, d_inputSize), d_fullProjection);
	ComputeProjection(d_ROI, d_roiProjection);
	ComputeProjection(Rect({0, 0}, d_viewSize), d_viewProjection);

	UploadHelpText();

	if (Watermark().empty() == false) {
		d_watermarkText = d_watermarkFont.Compile("TEST MODE");
	}

	d_promptBackground = std::make_unique<gl::VertexArrayObject>();

	// clang-format off
	float vertices[] = {
	    -1.0f, -1.0f, //
	     1.0f, -1.0f, //
	     1.0f,  1.0f, //
	     1.0f,  1.0f, //
	    -1.0f,  1.0f, //
	    -1.0f, -1.0f, //
	};
	// clang-format on
	d_promptBackground
	    ->BufferData<float, 2>(GL_STATIC_DRAW, vertices, sizeof(vertices));
}

void GLUserInterface::InitPBOs() {
	d_logger.DInfo("initializing frame buffer objects");

	for (auto &buffer : d_buffers) {
		buffer.Points = std::make_unique<fort::gl::VertexArrayObject>();
		glGenBuffers(1, &buffer.PBO);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.PBO);
		glBufferData(
		    GL_PIXEL_UNPACK_BUFFER,
		    d_workingSize.x() * d_workingSize.y(),
		    0,
		    GL_STREAM_DRAW
		);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLUserInterface::ComputeProjection(const Rect &roi, Eigen::Matrix3f &res) {
	res << 2.0f / float(roi.width()), 0.0f,
	    -1.0f - 2.0f * float(roi.x()) / float(roi.width()), // row 0
	    0.0f, -2.0f / float(roi.height()),
	    1.0f + 2.0f * float(roi.y()) / float(roi.height()), // row 1
	    0.0f, 0.0f, 1.0f;
}

void GLUserInterface::DrawMovieFrame(const DrawBuffer &buffer) {
	glUseProgram(d_frameProgram);
	if (buffer.FullUploaded == false) {
		slog::DDebug(
		    "Rendering full",
		    slog::Int("FrameID", buffer.Frame.FrameID),
		    slogRect("ROI", d_ROI),
		    slogRect("FrameROI", buffer.Frame.CurrentROI)
		);
		// we use an higher resolution zoomed frame, we therefore use the
		// full ROI projection
		fort::gl::Upload(d_frameProgram, "scaleMat", d_fullProjection);
	} else {
		slog::DDebug(
		    "Rendering Scaled",
		    slog::Int("FrameID", buffer.Frame.FrameID),
		    slogRect("ROI", d_ROI),
		    slogRect("FrameROI", buffer.Frame.CurrentROI)
		);
		fort::gl::Upload(d_frameProgram, "scaleMat", d_roiProjection);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, d_frameTexture);
	// glUniform1i(d_frameTexture, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.PBO);

	glTexImage2D(
	    GL_TEXTURE_2D,
	    0,
	    GL_R8,
	    d_workingSize.x(),
	    d_workingSize.y(),
	    0,
	    GL_RED,
	    GL_UNSIGNED_BYTE,
	    0
	);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glBindVertexArray(d_frameBuffer->VAO);
	Defer {
		glBindVertexArray(0);
	};
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GLUserInterface::UploadPoints(DrawBuffer &buffer) {
	if (!buffer.Frame.Message) {
		return;
	}

	std::vector<float> points;
	points.reserve(
	    buffer.Data.HighlightedIndexes.size() + buffer.Data.NormalIndexes.size()
	);
	buffer.Labels.clear();
	for (const auto hIndex : buffer.Data.HighlightedIndexes) {
		const auto &t = buffer.Frame.Message->tags(hIndex);
		points.push_back(t.x());
		points.push_back(t.y());
		buffer.Labels.emplace_back(std::make_tuple(
		    d_labelCache(t.id()),
		    Eigen::Vector2i{int(t.x()), int(t.y())}
		));
	}
	for (const auto nIndex : buffer.Data.NormalIndexes) {
		const auto &t = buffer.Frame.Message->tags(nIndex);
		points.push_back(t.x());
		points.push_back(t.y());
		buffer.Labels.emplace_back(std::make_tuple(
		    d_labelCache(t.id()),
		    Eigen::Vector2i{int(t.x()), int(t.y())}
		));
	}
	buffer.Points
	    ->BufferData<float, 2>(GL_DYNAMIC_DRAW, points.data(), points.size());
}

float GLUserInterface::FullToWindowScaleFactor() const {
	return std::min(
	    float(d_windowSize.x()) / float(d_inputSize.x()),
	    float(d_windowSize.y()) / float(d_inputSize.y())
	);
}

void GLUserInterface::DrawIndividualsROI(const DrawBuffer &buffer) {
	if (!buffer.Frame.Message || DisplayROI() == false ||
	    buffer.Points == nullptr) {
		return;
	}
	glUseProgram(d_roiProgram);

	auto factor = FullToWindowScaleFactor();
	if (d_ROI.Size() != d_inputSize) {
		factor *= float(d_inputSize.width()) / float(d_ROI.width());
	}
	auto floatPointSize = float(d_individualROISize) * factor;

	fort::gl::Upload(d_roiProgram, "scaleMat", d_roiProjection);

	glBindVertexArray(buffer.Points->VAO);
	Defer {
		glBindVertexArray(buffer.Points->VAO);
	};

	glPointSize(floatPointSize);
	fort::gl::Upload(d_roiProgram, "lineWidth", 6.0f / floatPointSize);

	fort::gl::Upload(d_roiProgram, "boxColor", POINT_NORMAL_COLOR);
	glDrawArrays(
	    GL_POINTS,
	    buffer.Data.HighlightedIndexes.size(),
	    buffer.Data.NormalIndexes.size()
	);

	fort::gl::Upload(d_roiProgram, "boxColor", POINT_HIGHLIGHTED_COLOR);
	glDrawArrays(GL_POINTS, 0, buffer.Data.HighlightedIndexes.size());
}

void GLUserInterface::DrawPoints(const DrawBuffer &buffer) {
	if (!buffer.Frame.Message) {
		return;
	}
	glUseProgram(d_pointProgram);
	auto factor = FullToWindowScaleFactor();
	if (d_ROI.Size() != d_inputSize) {
		factor *= float(d_inputSize.width()) / float(d_ROI.width());
	}
	fort::gl::Upload(d_pointProgram, "scaleMat", d_roiProjection);

	glBindVertexArray(buffer.Points->VAO);
	Defer {
		glBindVertexArray(buffer.Points->VAO);
	};

	glPointSize(float(NORMAL_POINT_SIZE) * factor);
	fort::gl::Upload(d_pointProgram, "circleColor", POINT_NORMAL_COLOR);
	glDrawArrays(
	    GL_POINTS,
	    buffer.Data.HighlightedIndexes.size(),
	    buffer.Data.NormalIndexes.size()
	);

	fort::gl::Upload(d_pointProgram, "circleColor", POINT_HIGHLIGHTED_COLOR);
	glPointSize(float(HIGHLIGHTED_POINT_SIZE) * factor);
	glDrawArrays(GL_POINTS, 0, buffer.Data.HighlightedIndexes.size());
}

void GLUserInterface::DrawLabels(const DrawBuffer &buffer) {
	if (DisplayLabels() == false || buffer.Labels.empty()) {
		return;
	}
	float factor = float(d_ROI.width()) / float(d_inputSize.width());
	d_logger.DDebug(
	    "drawing labels",
	    slog::Float("factor", factor),
	    slogSize("working", d_workingSize),
	    slogSize("ROI", d_ROI.Size())
	);
	gl::CompiledText::TextScreenPosition textArgs{
	    .ViewportSize =
	        {d_inputSize.width() * factor, d_inputSize.height() * factor},
	    .Size = LABEL_FONT_SIZE / FullToWindowScaleFactor(),
	};
	const auto &firstLabel = std::get<0>(buffer.Labels.front());
	firstLabel->SetColor(LABEL_FOREGROUND);
	firstLabel->SetBackgroundColor(LABEL_BACKGROUND);
	constexpr static int OFFSET = 0.4 * NORMAL_POINT_SIZE;
	for (const auto &[text, pos] : buffer.Labels) {
		textArgs.Position =
		    Eigen::Vector2f{pos.x() + OFFSET, pos.y() - OFFSET} -
		    d_ROI.TopLeft().cast<float>();
		text->Render(textArgs, true);
	}
}

template <typename T>
std::string printLine(const std::string &label, size_t size, const T &value) {
	std::ostringstream oss;
	oss << " " << label << ":" << std::setw(size - label.size() - 1)
	    << std::setfill(' ') << std::right << std::fixed << std::setprecision(2)
	    << value << " ";
	return oss.str();
}

void GLUserInterface::UploadInformations(DrawBuffer &buffer) {

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
	if (buffer.Frame.VideoOutputProcessed ==
	    std::numeric_limits<size_t>::max()) {
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

	oss << printLine(
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
	    << std::endl;

	buffer.Overlay = d_overlayFont.Compile(oss.str(), 3);
}

void GLUserInterface::DrawInformations(const DrawBuffer &buffer) {
	if (DisplayOverlay() == false) {
		return;
	}
	buffer.Overlay.SetBackgroundColor(OVERLAY_BACKGROUND);
	buffer.Overlay.SetColor(OVERLAY_FOREGROUND);
	buffer.Overlay.Render(
	    {
	        .ViewportSize = d_viewSize,
	        .Position     = {0, OVERLAY_FONT_SIZE},
	        .Size         = OVERLAY_FONT_SIZE,
	    },
	    true
	);
}

void GLUserInterface::Draw(const DrawBuffer &buffer) {
	if (!buffer.Frame.Full) {
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT);

	DrawMovieFrame(buffer);
	DrawPoints(buffer);
	DrawIndividualsROI(buffer);
	DrawLabels(buffer);
	DrawWatermark();
	DrawInformations(buffer);
	DrawHelp();
	DrawPrompt();
}

void GLUserInterface::DrawWatermark() {
	if (d_watermarkText.has_value() == false) {
		return;
	}
	const auto &watermarkText = d_watermarkText.value();

	auto bbox = watermarkText.BoundingBox(
	    {.ViewportSize = d_viewSize,
	     .Position     = {0, 0},
	     .Size         = WATERMARK_FONT_SIZE}
	);
	float w{bbox.z() - bbox.x()}, h{bbox.w() - bbox.y()};

	watermarkText.SetColor(WATERMARK_FOREGROUND);

	watermarkText.Render(
	    {
	        .ViewportSize = d_viewSize,
	        .Position =
	            {(d_viewSize.width() - w) / 2,
	             (d_viewSize.height() - h) / 2 - bbox.y()},
	        .Size = WATERMARK_FONT_SIZE,
	    },
	    false
	);
}

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

void GLUserInterface::UploadHelpText() {
	const static size_t COLS = 50;
	std::ostringstream  help;
	help << std::string(COLS + 2, ' ') << std::endl
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
	     << std::string(COLS + 2, ' ');

	d_helpText = d_overlayFont.Compile(help.str(), 1);
}

void GLUserInterface::DrawHelp() {
	if (DisplayHelp() == false) {
		return;
	}

	auto  bbox = d_helpText.BoundingBox({
	     .ViewportSize = d_viewSize,
	     .Position     = {0, 0},
	     .Size         = OVERLAY_FONT_SIZE,
    });
	float textWidth{bbox.z() - bbox.x()}, textHeight{bbox.w() - bbox.y()};
	d_helpText.SetColor(OVERLAY_FOREGROUND);
	d_helpText.SetBackgroundColor(OVERLAY_BACKGROUND);
	d_helpText.Render(
	    {
	        .ViewportSize = d_viewSize,
	        .Position =
	            {(d_viewSize.width() - textWidth) / 2,
	             (d_viewSize.height() - textHeight) / 2},
	        .Size = OVERLAY_FONT_SIZE,
	    },
	    true
	);
}

void GLUserInterface::DrawPrompt() {
	if (PromptAndValue().empty() == true) {
		return;
	}
	static Eigen::Matrix3f identity = Eigen::Matrix3f::Identity();

	glUseProgram(d_primitiveProgram);
	fort::gl::Upload(d_primitiveProgram, "primitiveColor", OVERLAY_BACKGROUND);
	fort::gl::Upload(d_primitiveProgram, "scaleMat", identity);
	glBindVertexArray(d_promptBackground->VAO);
	Defer {
		glBindVertexArray(0);
	};
	glDrawArrays(GL_TRIANGLES, 0, 6);
	d_promptText.SetColor(OVERLAY_FOREGROUND);
	d_promptText.Render({
	    .ViewportSize = d_viewSize,
	    .Position     = {10, OVERLAY_FONT_SIZE},
	    .Size         = OVERLAY_FONT_SIZE,
	});
}

const Eigen::Vector3f GLUserInterface::POINT_NORMAL_COLOR = {
    100.0f / 255.0f,
    143.0f / 255.0f,
    255.0f / 255.0f,
};
const Eigen::Vector3f GLUserInterface::POINT_HIGHLIGHTED_COLOR = {
    254.0f / 255.0f,
    097.0f / 255.0f,
    000.0f / 255.0f,
};

const Eigen::Vector4f GLUserInterface::OVERLAY_FOREGROUND = {
    1.0f, 1.0f, 1.0f, 1.0f
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

const Eigen::Vector4f GLUserInterface::WATERMARK_FOREGROUND = {
    255.0f / 255.0f,
    176.0f / 255.0f,
    000.0f / 255.0f,
    1.0f,
};

} // namespace artemis
} // namespace fort

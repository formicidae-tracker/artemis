#pragma once

#include <Eigen/Dense>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/util/Constants.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "UserInterface.hpp"
#include "fort/gl/TextRenderer.hpp"
#include "fort/gl/VAOPool.hpp"
#include "fort/utils/LRUCache.hpp"

#include <fort/gl/Window.hpp>
#include <memory>
#include <optional>

namespace fort {
namespace artemis {

class GLUserInterface : public UserInterface, public fort::gl::Window {
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	GLUserInterface(
	    const Size                   &workingResolution,
	    const Size                   &inputResolution,
	    const UserInterface::Options &options,
	    const ROIChannelPtr          &roiChannel
	);

	virtual ~GLUserInterface();

	void Task() override;

	void UpdateFrame(const FrameToDisplay &frame, const DataToDisplay &data)
	    override;

private:
	using CompiledTextPtr = std::shared_ptr<gl::CompiledText>;

	struct DrawBuffer {
		FrameToDisplay   Frame;
		DataToDisplay    Data;
		Size             TrackingSize;
		bool             FullUploaded;
		GLuint           PBO;
		gl::CompiledText Overlay;

		std::unique_ptr<fort::gl::VertexArrayObject>              Points;
		std::vector<std::tuple<CompiledTextPtr, Eigen::Vector2i>> Labels;
	};

	void InitGLData();
	void InitPBOs();
	void OnSizeChanged(int width, int height) override;
	void OnKey(int key, int scancode, int action, int mods) override;
	void OnText(unsigned int codepoint) override;
	void OnMouseMove(double x, double y) override;
	void OnMouseInput(int button, int action, int mods) override;
	void OnScroll(double xOffset, double yOffset) override;

	void Zoom(int increment);
	void Displace(const Eigen::Vector2f &offset);
	void UpdateROI(const Rect &ROI);
	void UploadHelpText();

	void ComputeViewport();

	void Draw() override;
	void UploadTexture(DrawBuffer &buffer);
	void UploadPoints(DrawBuffer &buffer);
	void UploadInformations(DrawBuffer &buffer);

	void Draw(const DrawBuffer &buffer);

	void DrawMovieFrame(const DrawBuffer &buffer);
	void DrawPoints(const DrawBuffer &buffer);
	void DrawLabels(const DrawBuffer &buffer);
	void DrawInformations(const DrawBuffer &buffer);
	void DrawWatermark();
	void DrawHelp();
	void DrawPrompt();
	void DrawIndividualsROI(const DrawBuffer &buffer);

	static void ComputeRectVertices(Eigen::MatrixXf &mat, const Rect &rect);

	static void ComputeProjection(const Rect &roi, Eigen::Matrix3f &res);
	void        UpdateProjections();

	float FullToWindowScaleFactor() const;

	DrawBuffer d_buffers[2];
	size_t     d_index;

	const Size   d_workingSize, d_inputSize;
	const float  d_inputToWorkingRatio;
	const size_t d_individualROISize;

	Size            d_windowSize, d_viewSize;
	int             d_currentScaleFactor;
	Eigen::Vector2i d_currentPOI;
	Rect            d_ROI;

	Eigen::Matrix3f d_fullProjection, d_viewProjection, d_roiProjection;

	GLuint d_frameProgram, d_frameTexture, d_pointProgram, d_primitiveProgram,
	    d_roiProgram;

	std::unique_ptr<fort::gl::VertexArrayObject> d_frameBuffer;

	slog::Logger<1> d_logger;

	gl::TextRenderer d_labelFont, d_overlayFont, d_watermarkFont;
	utils::LRUCache<256, std::function<CompiledTextPtr(uint32_t)>> d_labelCache;

	gl::CompiledText                       d_helpText;
	gl::CompiledText                       d_promptText;
	std::optional<gl::CompiledText>        d_watermarkText;
	std::unique_ptr<gl::VertexArrayObject> d_promptBackground;

	struct MouseDragStart {
		Eigen::Vector2d Mouse;
		Eigen::Vector2i POI;
	};

	Eigen::Vector2d               d_mousePosition;
	std::optional<MouseDragStart> d_mouseDrag;

	static const Eigen::Vector4f OVERLAY_FOREGROUND;
	static const Eigen::Vector4f OVERLAY_BACKGROUND;
	static const Eigen::Vector4f WATERMARK_FOREGROUND;
	static const Eigen::Vector4f LABEL_FOREGROUND;
	static const Eigen::Vector4f LABEL_BACKGROUND;
	//	static const Eigen::Vector3f POINT_NORMAL_COLOR;
	// static const Eigen::Vector3f POINT_HIGHLIGHTED_COLOR;

	constexpr static size_t OVERLAY_COLS           = 30;
	constexpr static size_t OVERLAY_ROWS           = 8;
	constexpr static size_t NORMAL_POINT_SIZE      = 70;
	constexpr static size_t HIGHLIGHTED_POINT_SIZE = 100;
	constexpr static size_t LABEL_FONT_SIZE          = 16;
	constexpr static size_t OVERLAY_FONT_SIZE        = 14;
	constexpr static size_t WATERMARK_FONT_SIZE      = 96;
};

} // namespace artemis
} // namespace fort

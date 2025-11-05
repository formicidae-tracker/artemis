#include "ProcessFrameTask.hpp"

#include <cstdint>

#include <fort/hermes/FrameReadout.pb.h>
#include <slog++/Attribute.hpp>

#include <artemis-config.h>
#include <sstream>

#include "ApriltagDetector.hpp"
#include "Connection.hpp"
#include "ImageU8.hpp"
#include "UserInterfaceTask.hpp"

namespace fort {
namespace artemis {

Size workingResolution(
    const Size &windowResolution, const Size &inputResolution
) {
	return {
	    int(inputResolution.width() * float(windowResolution.height()) /
	        float(inputResolution.height())),
	    windowResolution.height()
	};
}

ProcessFrameTask::ProcessFrameTask(
    const Options           &options,
    boost::asio::io_context &context,
    const Size              &inputResolution
)
    : d_config{options}
    , d_maximumThreads{std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() - 2 : 1}
    , d_workingResolution{workingResolution({1920, 1080}, inputResolution)}
    , d_executor{d_maximumThreads}
    , d_logger{slog::With(slog::String("task", "process"))}
    , d_current{} {
	d_actualThreads = d_maximumThreads;

	SetUpDetection(inputResolution, options.Apriltag);
	SetUpUserInterface(d_workingResolution, inputResolution, options);
	SetUpVideoOutputTask(options.VideoOutput, context, options.LegacyMode);
	SetUpCataloguing(options);
	SetUpConnection(options.Leto, context);

	std::string ids, prefix;
	for (const auto &id : options.Process.FrameIDs()) {
		ids += prefix + std::to_string(id);
		prefix = ",";
	}
	d_logger.Info(
	    "processing",
	    slog::String("IDs", ids),
	    slog::Int("stride", options.Process.FrameStride)
	);

	SetUpTaskflow();
}

void ProcessFrameTask::SetUpTaskflow() {
	d_taskflow.emplace([]() {
		// TODO send to video output;
	});

	auto overflowCondition =
	    d_taskflow.emplace([this]() { return d_frameQueue.peek() != nullptr; }
	    ).name("overflowCondition");

	auto dropCondition =
	    d_taskflow
	        .emplace([this]() { return ShouldProcess(d_current.Frame->ID()); })
	        .name("dropCondition");

	auto [init, noDetection] = d_taskflow.emplace(
	    [this]() { d_current.AsImage = d_current.Frame->ToImageU8(); },
	    []() { return 0; }
	);
	init.name("init");
	noDetection.name("noDetection");

	auto prepare = d_taskflow
	                   .emplace([this]() {
		                   d_current.Readout = d_messagePool->Get(
		                   ); // otherwise we keep the last one.
		                   PrepareMessage(d_current.Frame, *d_current.Readout);
		                   d_detector->SetInputOutput(
		                       d_current.AsImage,
		                       d_current.Readout.get()
		                   );
	                   })
	                   .name("prepare");

	auto dropFrame = d_taskflow
	                     .emplace([this]() {
		                     DropFrame(d_current.Frame);
		                     return 0;
	                     })
	                     .name("drop");

	auto detect = d_taskflow.composed_of(d_detector->Taskflow()).name("detect");
	auto detectionDone =
	    d_taskflow.emplace([]() { return 0; }).name("detectionDone");

	init.precede(dropCondition);
	dropCondition.precede(noDetection, overflowCondition);
	overflowCondition.precede(prepare, dropFrame);
	prepare.precede(detect);
	detect.precede(detectionDone);

	if (d_connection) {
		auto upstream = d_taskflow.emplace([this]() {
			Connection::PostMessage(d_connection, *d_current.Readout);
		});
		upstream.succeed(detect);
	}

	if (d_config.NewAntOutputDir.empty() == false) {
		auto exportFullFrame =
		    d_taskflow
		        .emplace([this]() {
			        if (d_current.Frame->Time().Before(d_nextFrameExport)) {
				        return;
			        }
			        d_detector->SetMaxConcurrency(d_maximumThreads - 1);
			        d_executor.silent_async([this, frame = d_current.Frame]() {
				        ExportFullFrame(d_current.Frame);
				        d_detector->SetMaxConcurrency(d_maximumThreads);
			        });
		        })
		        .name("exportFull");
		auto catalogAnt =
		    d_taskflow
		        .emplace([this](tf::Runtime &rt) {
			        CatalogAnt(d_current.Frame, d_current.Readout, rt);
		        })
		        .name("catalogIndividuals");
		detect.precede(catalogAnt, exportFullFrame);
	}

	if (d_userInterface) {
		auto fullResize = d_taskflow
		                      .emplace([this]() {
			                      d_current.Full = d_imagePool->Get(
			                          d_workingResolution.width(),
			                          d_workingResolution.height()
			                      );
			                      ImageU8::Resize(
			                          *d_current.Full,
			                          d_current.AsImage,
			                          ImageU8::ScaleMode::None
			                      );
		                      })
		                      .name("fullScaleDown");
		auto zoomResize =
		    d_taskflow
		        .emplace([this]() {
			        d_wantedROI = d_userInterface->UpdateROI(d_wantedROI);
			        if (d_wantedROI.Size() == d_current.AsImage.Size()) {
				        d_current.Zoomed = nullptr;
			        }
			        d_current.Zoomed = d_imagePool->Get(
			            d_workingResolution.width(),
			            d_workingResolution.height()
			        );
			        ImageU8::Resize(
			            *d_current.Zoomed,
			            d_current.AsImage.GetROI(d_wantedROI),
			            ImageU8::ScaleMode::None
			        );
		        })
		        .name("ROIScaleDown");

		auto display = d_taskflow
		                   .emplace([this]() {
			                   DisplayFrame(d_current.Frame, d_current.Readout);
		                   })
		                   .name("display");
		init.precede(fullResize, zoomResize);
		// will only display if the resize are done, or either detection or
		// noDetection or dropped frame is done.
		display.succeed(
		    fullResize,
		    zoomResize,
		    detectionDone,
		    noDetection,
		    dropFrame
		);
	}
}

void ProcessFrameTask::ExportFullFrame(const Frame::Ptr &frame) {
	d_nextFrameExport = d_nextFrameExport.Add(d_config.ImageRenewPeriod);
	std::ostringstream oss;
	oss << "frame_" << frame->ID() << ".png";
	auto filepath = d_config.NewAntOutputDir / oss.str();
	d_logger.Info(
	    "full frame export",
	    slog::String("path", filepath.string()),
	    slog::Int("ID", frame->ID())
	);
	frame->ToImageU8().WritePNG(filepath);
}

UserInterfaceTaskPtr ProcessFrameTask::UserInterfaceTask() const {
	return d_userInterface;
}

void ProcessFrameTask::SetUpVideoOutputTask(
    const VideoOutputOptions &options,
    boost::asio::io_context  &context,
    bool                      legacyMode
) {
	if (options.ToStdout == false) {
		return;
	}
}

void ProcessFrameTask::SetUpDetection(
    const Size &inputResolution, const ApriltagOptions &options
) {
	if (options.Family() == tags::Family::Undefined) {
		return;
	}
	d_detector = std::make_unique<ApriltagDetector>(
	    d_maximumThreads,
	    inputResolution,
	    options
	);
}

void ProcessFrameTask::SetUpCataloguing(const Options &options) {
	if (options.NewAntOutputDir.empty()) {
		return;
	}

	d_nextAntCatalog  = Time::Now();
	d_nextFrameExport = d_nextAntCatalog.Add(10 * Duration::Second);
}

void ProcessFrameTask::SetUpConnection(
    const LetoOptions &options, boost::asio::io_context &context
) {
	if (options.Host.empty()) {
		return;
	}
	d_connection = Connection::Create(
	    context,
	    options.Host,
	    options.Port,
	    5 * Duration::Second
	);
}

void ProcessFrameTask::SetUpUserInterface(
    const Size    &workingResolution,
    const Size    &fullResolution,
    const Options &options
) {
	d_userInterface = std::make_shared<artemis::UserInterfaceTask>(
	    workingResolution,
	    fullResolution,
	    options
	);

	d_wantedROI = d_userInterface->DefaultROI();
}

ProcessFrameTask::~ProcessFrameTask() {}

void ProcessFrameTask::TearDown() {
	if (d_userInterface) {
		d_userInterface->CloseQueue();
	}
}

void ProcessFrameTask::Run() {
	d_logger.Info("started");
	d_frameDropped   = 0;
	d_frameProcessed = 0;
	d_start          = Time::Now();
	for (;;) {
		d_frameQueue.wait_dequeue(d_current.Frame);
		if (!d_current.Frame) {
			break;
		}
		d_executor.run(d_taskflow).wait();
		// release all memory from here.
		d_current.Frame   = nullptr;
		d_current.AsImage = ImageU8{};
	}
	d_logger.Info("tear down");
	TearDown();
	d_logger.Info("end");
}

void ProcessFrameTask::DropFrame(const Frame::Ptr &frame) {
	++d_frameDropped;
	d_logger.Warn(
	    "frame dropped due to over-processing",
	    slog::Int("total_dropped", d_frameDropped.load()),
	    slog::Float(
	        "percent_dropped",
	        100.0 * float(d_frameDropped.load()) /
	            float(d_frameDropped.load() + d_frameProcessed.load())
	    )
	);

	if (!d_connection) {
		return;
	}

	d_current.Readout = d_messagePool->Get();

	PrepareMessage(frame, *d_current.Readout);
	d_current.Readout->set_error(hermes::FrameReadout::PROCESS_OVERFLOW);

	Connection::PostMessage(d_connection, *d_current.Readout);
}

void ProcessFrameTask::ProcessFrame(const Frame::Ptr &frame) {}

void ProcessFrameTask::PrepareMessage(
    const Frame::Ptr &frame, hermes::FrameReadout &m
) {
	m.Clear();
	m.set_timestamp(frame->Timestamp());
	m.set_frameid(frame->ID());
	frame->Time().ToTimestamp(m.mutable_time());
	m.set_producer_uuid(d_config.UUID);
	m.set_width(frame->Width());
	m.set_height(frame->Height());
}

bool ProcessFrameTask::ShouldProcess(uint64_t ID) {
	if (d_config.FrameStride <= 1) {
		return true;
	}
	return d_config.FrameIDs.count(ID % d_config.FrameStride) != 0;
}

void ProcessFrameTask::QueueFrame(const Frame::Ptr &frame) {
	d_frameQueue.enqueue(frame);
}

void ProcessFrameTask::CloseFrameQueue() {
	d_frameQueue.enqueue(nullptr);
}

void ProcessFrameTask::CatalogAnt(
    Frame::Ptr frame, std::shared_ptr<hermes::FrameReadout> m, tf::Runtime &rt
) {
	if (d_config.NewAntOutputDir.empty()) {
		return;
	}

	ResetExportedID(frame->Time());

	auto toExport = FindUnexportedID(*m);

	for (auto [tagID, x, y] : toExport) {
		rt.silent_async([image   = frame->ToImageU8(),
		                 frameID = frame->ID(),
		                 tagID,
		                 x,
		                 y,
		                 this]() {
			this->ExportROI(image, frameID, tagID, x, y);
		});
	}
	rt.corun();

	for (const auto &[tagID, x, y] : toExport) {
		d_exportedID.insert(tagID);
	}
}

void ProcessFrameTask::ResetExportedID(const Time &time) {
	if (d_nextAntCatalog.Before(time)) {
		d_nextAntCatalog = time.Add(d_config.ImageRenewPeriod);
		d_exportedID.clear();
	}
}

std::vector<std::tuple<uint32_t, double, double>>
ProcessFrameTask::FindUnexportedID(const hermes::FrameReadout &m) {
	std::vector<std::tuple<uint32_t, double, double>> res;
	for (const auto &t : m.tags()) {
		if (d_exportedID.count(t.id()) != 0) {
			continue;
		}
		res.push_back({t.id(), t.x(), t.y()});
		if (res.size() >= d_actualThreads) {
			break;
		}
	}
	return res;
}

void ProcessFrameTask::ExportROI(
    const ImageU8 &image, uint64_t frameID, uint32_t tagID, double x, double y
) {
	std::ostringstream oss;
	oss << "ant" << tagID << "_" << frameID << ".png";

	auto roi = GetROICenteredAt(
	    {int(x), int(y)},
	    Size(d_config.NewAntROISize, d_config.NewAntROISize),
	    {image.width, image.height}
	);
	image.GetROI(roi).WritePNG(d_config.NewAntOutputDir / oss.str());
}

void ProcessFrameTask::DisplayFrame(
    const Frame::Ptr &frame, const std::shared_ptr<hermes::FrameReadout> &m
) {

	UserInterface::FrameToDisplay toDisplay = {
	    .Full                 = std::move(d_current.Full),
	    .Zoomed               = std::move(d_current.Zoomed),
	    .Message              = m,
	    .CurrentROI           = d_wantedROI,
	    .FrameTime            = frame->Time(),
	    .FPS                  = CurrentFPS(frame->Time()),
	    .FrameProcessed       = d_frameProcessed,
	    .FrameDropped         = d_frameDropped,
	    .VideoOutputProcessed = -1UL,
	    .VideoOutputDropped   = -1UL,
	};

	d_userInterface->QueueFrame(toDisplay);
}

double ProcessFrameTask::CurrentFPS(const Time &time) {
	return d_frameProcessed / time.Sub(d_start).Seconds();
}

} // namespace artemis
} // namespace fort

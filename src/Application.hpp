#pragma once

#include "Options.hpp"

#include <memory>

namespace fort {
namespace artemis {

class FrameGrabber;

class Application {
public:
	static void Execute(int argc, char ** argv);



private :
	static bool InterceptCommand(const Options & options);

	static std::shared_ptr<FrameGrabber> LoadFramegrabber(const std::string & stubImagePath,
	                                                      const CameraOptions & camera);

	static void InitGlobalDependencies();
	static void InitGoogleLogging(const std::string & applicationName,
	                              const GeneralOptions & options);

	Application(const Options & options);

	void Run();

};

} // namespace artemis
} // namespace fort

include(CheckCXXSourceRuns)

Check_CXX_Source_Runs(
"
#include <system_error>


int main() {
	if (std::error_code(EDOM,std::system_category()) == std::errc::argument_out_of_domain ) {
		return 0;
	}
	return 1;
}
"
ARTEMIS_USE_SYSTEM_CATEGORY)


configure_file(config.h.in artemis-config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}
	                ${CMAKE_CURRENT_SOURCE_DIR}
	                )



add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ui/shaders_data.c ${CMAKE_CURRENT_BINARY_DIR}/ui/shaders_data.h
                   COMMAND ${RESOURCIFY_EXECUTABLE} ARGS "-O" ${CMAKE_CURRENT_BINARY_DIR}/ui/shaders_data
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/frame.vertexshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/frame.fragmentshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/primitive.vertexshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/primitive.fragmentshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/circle.fragmentshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/font.vertexshader
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/font.fragmentshader


                   DEPENDS resourcify
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/frame.vertexshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/frame.fragmentshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/primitive.vertexshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/circle.fragmentshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/font.vertexshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/font.fragmentshader
                           ${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/primitive.fragmentshader
                           )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts/fonts_data.c ${CMAKE_CURRENT_BINARY_DIR}/fonts/fonts_data.h
                   COMMAND ${RESOURCIFY_EXECUTABLE} ARGS "-O" ${CMAKE_CURRENT_BINARY_DIR}/fonts/fonts_data
                                                         "-I" ${CMAKE_CURRENT_SOURCE_DIR}/fonts/vga.fon

                   DEPENDS resourcify
                           ${CMAKE_CURRENT_SOURCE_DIR}/fonts/vga.fon
                           )



set(SRC_FILES utils/Defer.cpp
	          utils/PosixCall.cpp
	          utils/FlagParser.cpp
	          utils/StringManipulation.cpp
	          utils/Partitions.cpp
	          Task.cpp
	          Time.cpp
	          Options.cpp
	          FrameGrabber.cpp
	          StubFrameGrabber.cpp
	          Connection.cpp
	          Application.cpp
	          AcquisitionTask.cpp
	          ProcessFrameTask.cpp
	          ApriltagDetector.cpp
	          FullFrameExportTask.cpp
	          UserInterfaceTask.cpp
	          VideoOutputTask.cpp
	          ImageTextRenderer.cpp
	          ui/UserInterface.cpp
	          ui/StubUserInterface.cpp
	          ui/GLVertexBufferObject.cpp
	          ui/GLUserInterface.cpp
	          ui/ShaderUtils.cpp
	          ui/shaders_data.c
	          fonts/fonts_data.c
	          ui/GLFont.cpp
	          main.cpp
	          )


set(HDR_FILES utils/Defer.hpp
              utils/PosixCall.hpp
              utils/PosixCall.hpp
	          utils/FlagParser.hpp
	          utils/StringManipulation.hpp
	          utils/Partitions.hpp
	          Task.hpp
	          FrameGrabber.hpp
	          Time.hpp
	          Connection.hpp
	          StubFrameGrabber.hpp
	          Options.hpp
	          AcquisitionTask.hpp
	          ProcessFrameTask.hpp
	          ApriltagDetector.hpp
	          FullFrameExportTask.hpp
	          UserInterfaceTask.hpp
	          VideoOutputTask.hpp
	          ImageTextRenderer.hpp
	          ui/UserInterface.hpp
	          ui/StubUserInterface.hpp
	          ui/GLVertexBufferObject.hpp
	          ui/GLUserInterface.hpp
	          ui/ShaderUtils.hpp
	          ui/shaders_data.h
	          fonts/fonts_data.h
	          ui/GLFont.hpp
	          Application.hpp
	          )


set(UTEST_SRC_FILES main-check.cpp
                    utils/DeferUTest.cpp
                    utils/FlagParserUTest.cpp
                    utils/StringManipulationUTest.cpp
                    TimeUTest.cpp
	                ConnectionUTest.cpp
	                utils/PartitionsUTest.cpp
	                OptionsUTest.cpp
	                TaskUTest.cpp
	                ObjectPoolUTest.cpp
	                )

set(UTEST_HDR_FILES utils/DeferUTest.hpp
	                utils/FlagParserUTest.hpp
	                utils/StringManipulationUTest.hpp
	                utils/PartitionsUTest.hpp
	                TimeUTest.hpp
	                ConnectionUTest.hpp
	                OptionsUTest.hpp
	                TaskUTest.hpp
	                ObjectPoolUTest.hpp
	                )

if(EGrabber_FOUND)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.hpp)
endif(EGrabber_FOUND)


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 fort-tags
	                                 fort-hermes-cpp
	                                 ${PROTOBUF_LIBRARIES}
	                                 ${OpenCV_LIBS}
	                                 ${TBB_LIBRARIES}
	                                 glfw
	                                 ${GLEW_LIBRARIES}
	                                 OpenGL::OpenGL
	                                 freetype-gl
	                                 ${FONTCONFIG_LIBRARIES}
	                                 boost_system
	                                  "-ldl")

add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES gmock artemis-common)

install(TARGETS artemis DESTINATION bin)

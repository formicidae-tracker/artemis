set(SRC_FILES Defer.cpp
	          PosixCall.cpp
	          FlagParser.cpp
	          StringManipulation.cpp
	          CPUMap.cpp
	          Partitions.cpp
	          )




set(HDR_FILES Defer.h
	          PosixCall.h
	          PosixCall.h
	          FlagParser.h
	          StringManipulation.h
	          CPUMap.h
	          Partitions.h
	          )


set(UTEST_SRC_FILES ../main-check.cpp
	                DeferUTest.cpp
	                FlagParserUTest.cpp
	                StringManipulationUTest.cpp
	                PartitionsUTest.cpp
	                )

set(UTEST_HDR_FILES DeferUTest.h
	                FlagParserUTest.h
	                StringManipulationUTest.h
	                PartitionsUTest.h
	                )

add_library(utils STATIC ${SRC_FILES} ${HDR_FILES})

add_check_test(NAME utils
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES})

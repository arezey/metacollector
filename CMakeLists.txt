cmake_minimum_required (VERSION 2.8)
project (metacollector)

if (CMAKE_COMPILER_IS_GNUCXX)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Werror=switch -Werror=uninitialized -Werror=address -Werror=return-type -Werror=maybe-uninitialized -Werror=format -Werror=double-promotion -std=c++0x")
endif()

set (TEST_SOURCES
	src/format.cc
	src/lexer.cc
	src/lexerscanner.cc
	src/main.cc
	src/string.cc
)

set (TEST_HEADERS
	src/containers.h
	src/format.h
	src/lexer.h
	src/lexerscanner.h
	src/macros.h
	src/main.h
	src/string.h
	src/tokens.h
	src/types.h
)

add_executable (metacollector ${TEST_SOURCES})
get_target_property (NAMEDENUMS_EXE metacollector LOCATION)
install (TARGETS metacollector RUNTIME DESTINATION bin)

add_subdirectory (testcase)
cmake_minimum_required (VERSION 2.8)

set (TESTCASE_SOURCES
	testcase_main.cc
)

set (TESTCASE_HEADERS
	testcase_main.h
)

get_target_property (METACOLLECTOR_EXE metacollector LOCATION)
add_custom_target (testcase_metacollection ALL
    COMMAND ${METACOLLECTOR_EXE} ${TESTCASE_HEADERS} testcase_metadata.h testcase_metadata.cc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/testcase
    DEPENDS metacollector)

add_executable (testcase ${TESTCASE_SOURCES} testcase_metadata.cc)
add_dependencies(testcase testcase_metacollection)
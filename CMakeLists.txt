cmake_minimum_required(VERSION 2.8)

project(libpegmatite)

set(libpegmatite 0.1)

set(libpegmatite_CXX_SRCS
	ast.cc
	parser.cc
)

add_library(pegmatite SHARED ${libpegmatite_CXX_SRCS})
add_library(pegmatite-static STATIC ${libpegmatite_CXX_SRCS})
set_target_properties(pegmatite-static PROPERTIES
	POSITION_INDEPENDENT_CODE true
	OUTPUT_NAME "pegmatite")

if (CMAKE_BUILD_TYPE MATCHES "Debug")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CLANG_FLAGS "-Weverything -Wno-c++98-compat -Wno-padded")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_FLAGS}")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

option(USE_RTTI "Use native C++ RTTI" ON)
option(BUILD_DOCUMENTATION "Use Doxygen to create the HTML based API documentation" OFF)
if (USE_RTTI)
	add_definitions(-DUSE_RTTI=1)
endif()
if(BUILD_DOCUMENTATION)
	FIND_PACKAGE(Doxygen)
	if (NOT DOXYGEN_FOUND)
		message(FATAL_ERROR 
			"Doxygen is needed to build the documentation. Please install it correctly")
	endif()
	#-- Configure the Template Doxyfile for our specific project
	configure_file(Doxyfile.in 
	               ${PROJECT_BINARY_DIR}/Doxyfile @ONLY IMMEDIATE)
	#-- Add a custom target to run Doxygen when ever the project is built
	add_custom_target (Docs ALL 
	                   COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
	                   SOURCES ${PROJECT_BINARY_DIR}/Doxyfile)
endif()

option(BUILD_TRACING "Build with debug parse tracing support" OFF)
if (BUILD_TRACING)
	add_definitions("-D DEBUG_PARSING")
endif()

include_directories(.)
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

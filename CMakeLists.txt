# filepath: /home/bartosz/Documents/PROJECT/MPEG-TS-Transport-Stream-Parser/CMakeLists.txt
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
set(PROJECT_NAME "TS-PARSER")
project(${PROJECT_NAME})

# force static runtime libraries for msvc builds
if(MSVC)
  set(variables CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
  foreach(variable ${variables})
  if(${variable} MATCHES "/MD")
    string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
  endif()
  endforeach()
endif()

# set c++17
set (CMAKE_CXX_STANDARD 17)
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# compile everything position independent (even static libraries)
set( CMAKE_POSITION_INDEPENDENT_CODE TRUE )

# set include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# set verbose compile options
#set( CMAKE_VERBOSE_MAKEFILE ON )

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /PROFILE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

set(PROJECT_HEADERS
  include/tsCommon.h
  include/tsTransportStream.h)

set(PROJECT_SOURCES  
  src/TS_parser.cpp
  src/tsTransportStream.cpp)

source_group("Header Files" FILES ${PROJECT_HEADERS})
source_group("Source Files" FILES ${PROJECT_SOURCES})

add_executable(${PROJECT_NAME} ${PROJECT_HEADERS} ${PROJECT_SOURCES})



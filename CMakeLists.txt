CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)
PROJECT(executor)

IF(NOT DEFINED BUILD_TEST)
  SET(BUILD_TEST OFF)
ENDIF()

IF(${BUILD_TEST})
  ENABLE_TESTING()
ENDIF()

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED on)

SET(VENDOR_DIR common/hydro-vendor)

IF(${CMAKE_CXX_COMPILER} STREQUAL "/usr/bin/clang++") 
  SET(CMAKE_CXX_FLAGS_COMMON
      "-std=c++11 \
      -stdlib=libc++ -pthread")
ENDIF()

IF(${CMAKE_CXX_COMPILER} STREQUAL "/usr/bin/g++") 
  SET(CMAKE_CXX_FLAGS_COMMON
      "-std=c++11 -pthread")
ENDIF()

SET(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} \
    ${CMAKE_CXX_FLAGS_COMMON} \
    -g -O0 -fprofile-arcs -ftest-coverage")

SET(CMAKE_CXX_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE} \
    ${CMAKE_CXX_FLAGS_COMMON} \
    -O3")

ADD_SUBDIRECTORY(${VENDOR_DIR}/spdlog)
ADD_SUBDIRECTORY(${VENDOR_DIR}/yamlcpp)
ADD_SUBDIRECTORY(${VENDOR_DIR}/zeromq)
ADD_SUBDIRECTORY(${VENDOR_DIR}/zeromqcpp)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${SPDLOG_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${ZEROMQCPP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${ZEROMQ_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${YAMLCPP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(common/include)
# INCLUDE_DIRECTORIES(include)

INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER 
  ./common/proto/common.proto
  ./common/proto/kvs.proto
  ./common/proto/operation.proto
  ./common/proto/anna.proto
)


# Need to build a target at this level or subdirs won't have the 
# protobuf files generated.
ADD_LIBRARY(faas-proto ${PROTO_HEADER} ${PROTO_SRC})

FILE(GLOB_RECURSE ZMQ_UTIL_SRC common/include/zmq/*.cpp)
FILE(GLOB_RECURSE ZMQ_UTIL_HEADER common/include/zmq/*.hpp)
ADD_LIBRARY(hydro-zmq STATIC ${ZMQ_UTIL_HEADER} ${ZMQ_UTIL_SRC})
ADD_DEPENDENCIES(hydro-zmq zeromq zeromqcpp spdlog) 

# IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug") 
#   INCLUDE(common/cmake/clang-format.cmake)
#   INCLUDE(common/cmake/CodeCoverage.cmake) 
# ENDIF()

# share memory lib
SET(SHM_DIR ./common/shm-ipc)

ADD_SUBDIRECTORY(${SHM_DIR})

include_directories(
    ${SHM_DIR}/include 
    ${SHM_DIR}/3rdparty
    ${SHM_DIR}/src
    /usr/local/lib/python3.6/dist-packages/numpy/core/include
    /usr/include/python3.6)

if(UNIX)
  file(GLOB_RECURSE SHM_SRC_FILES ${SHM_DIR}/src/libipc/platform/*_linux.cpp)
    # ${SHM_DIR}/demo/PyCpp/Client.cpp
    # ${SHM_DIR}/demo/PyCpp/util.cpp
else()
  file(GLOB_RECURSE SHM_SRC_FILES ${SHM_DIR}/src/libipc/platform/*_win.cpp)
endif()
aux_source_directory(${SHM_DIR}/src SHM_SRC_FILES)
file(GLOB_RECURSE SHM_HEAD_FILES
  ${SHM_DIR}/include/libipc/*.h
  ${SHM_DIR}/src/libipc/*.h
  ${SHM_DIR}/src/libipc/*.inc
  ${SHM_DIR}/src/libipc/circ/*.h
  ${SHM_DIR}/src/libipc/memory/*.h
  ${SHM_DIR}/src/libipc/platform/*.h
  ${SHM_DIR}/src/libipc/utility/*.h
  ${SHM_DIR}/src/demo/PyCpp/*.h)

ADD_LIBRARY(shm-ipc ${SHM_SRC_FILES} ${SHM_HEAD_FILES})

LINK_DIRECTORIES(${ZEROMQ_LINK_DIRS} ${YAMLCPP_LINK_DIRS})

ADD_SUBDIRECTORY(src)
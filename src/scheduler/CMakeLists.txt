
CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/target)

INCLUDE_DIRECTORIES(../../include/scheduler)

SET(SCHEDULER_SOURCE 
    scheduler_server.cpp
    object_handlers.cpp
)

ADD_EXECUTABLE(scheduler ${SCHEDULER_SOURCE})
TARGET_LINK_LIBRARIES(scheduler ${CACHE_DEPENDENCIES})
TARGET_LINK_LIBRARIES(scheduler shm-ipc rt)
ADD_DEPENDENCIES(scheduler hydro-zmq zeromq zeromqcpp shm-ipc)
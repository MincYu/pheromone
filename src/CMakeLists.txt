CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

SET(CACHE_DEPENDENCIES
    protobuf
    faas-proto
    pthread
    zmq
    hydro-zmq
    yaml-cpp
)

ADD_SUBDIRECTORY(coordinator)
ADD_SUBDIRECTORY(executor)
ADD_SUBDIRECTORY(scheduler)
ADD_SUBDIRECTORY(manager)
ADD_SUBDIRECTORY(client)

CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/target)

INCLUDE_DIRECTORIES(../../include/coordinator)

SET(KV_LIBRARY_DEPENDENCIES
    protobuf::libprotobuf
    faas-proto
    pthread
    zmq
    hydro-zmq
    yaml-cpp
)

SET(COORDINATOR_SOURCE 
    coordinator.cpp
    notify_handler.cpp
    query_handler.cpp
    bucket_op_handler.cpp
    trigger_op_handler.cpp
    func_call_handler.cpp
    app_register_handler.cpp
)

ADD_EXECUTABLE(coordinator ${COORDINATOR_SOURCE})
TARGET_LINK_LIBRARIES(coordinator ${KV_LIBRARY_DEPENDENCIES})
ADD_DEPENDENCIES(coordinator zeromq zeromqcpp)

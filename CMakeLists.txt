cmake_minimum_required(VERSION 3.20)

option(ANOLIS_PROVIDER_EZO_WARNINGS_AS_ERRORS "Treat first-party compiler warnings as errors" OFF)

project(anolis_provider_ezo VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Warnings.cmake)

find_package(Protobuf REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(NOT DEFINED EZO_DRIVER_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ezo-driver/CMakeLists.txt")
        set(EZO_DRIVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/ezo-driver" CACHE PATH "Path to ezo-driver repository")
    else()
        set(EZO_DRIVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ezo-driver" CACHE PATH "Path to ezo-driver repository")
    endif()
endif()
if(NOT EXISTS "${EZO_DRIVER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ezo-driver not found at: ${EZO_DRIVER_DIR}\n"
        "Clone it next to this repo, or set -DEZO_DRIVER_DIR=/path/to/ezo-driver")
endif()

set(EZO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(EZO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(EZO_BUILD_ARDUINO_ADAPTER OFF CACHE BOOL "" FORCE)
set(EZO_BUILD_LINUX_ADAPTER OFF CACHE BOOL "" FORCE)
set(EZO_BUILD_POSIX_UART_ADAPTER OFF CACHE BOOL "" FORCE)
add_subdirectory("${EZO_DRIVER_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/ezo-driver" EXCLUDE_FROM_ALL)

if(NOT DEFINED ANOLIS_PROTOCOL_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/anolis-protocol/proto/anolis/deviceprovider/v1/envelope.proto")
        set(ANOLIS_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/anolis-protocol" CACHE PATH "Path to anolis-protocol repository")
    else()
        set(ANOLIS_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../anolis-protocol" CACHE PATH "Path to anolis-protocol repository")
    endif()
endif()

get_filename_component(ANOLIS_PROTOCOL_DIR_ABS "${ANOLIS_PROTOCOL_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ADPP_PROTO_IMPORT_ROOT "${ANOLIS_PROTOCOL_DIR_ABS}/proto")
set(ADPP_PROTO_V1_DIR "${ADPP_PROTO_IMPORT_ROOT}/anolis/deviceprovider/v1")
set(ADPP_PROTO_ENVELOPE "${ADPP_PROTO_V1_DIR}/envelope.proto")

if(NOT EXISTS "${ADPP_PROTO_ENVELOPE}")
    message(FATAL_ERROR
        "ADPP proto not found at: ${ADPP_PROTO_ENVELOPE}\n"
        "Expected anolis-protocol at: ${ANOLIS_PROTOCOL_DIR_ABS}\n"
        "Run: git submodule update --init --recursive\n"
        "Or set custom path: cmake -DANOLIS_PROTOCOL_DIR=/path/to/anolis-protocol ...")
endif()

file(GLOB ADPP_PROTOS "${ADPP_PROTO_V1_DIR}/*.proto")
if(NOT ADPP_PROTOS)
    message(FATAL_ERROR
        "No ADPP v1 proto files found under: ${ADPP_PROTO_V1_DIR}\n"
        "Expected split protocol layout from anolis-protocol PR #1.")
endif()

add_library(adpp_proto OBJECT ${ADPP_PROTOS})
target_link_libraries(adpp_proto PUBLIC protobuf::libprotobuf)
target_include_directories(adpp_proto SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

protobuf_generate(
    TARGET adpp_proto
    LANGUAGE cpp
    IMPORT_DIRS "${ADPP_PROTO_IMPORT_ROOT}"
    PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Compatibility shim: keep existing includes of "protocol.pb.h" working after proto split.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/protocol.pb.h"
"#pragma once\n"
"#include \"anolis/deviceprovider/v1/call.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/envelope.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/handshake.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/health.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/inventory.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/readiness.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/status.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/telemetry.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/types.pb.h\"\n"
"#include \"anolis/deviceprovider/v1/value.pb.h\"\n")

add_library(anolis_provider_ezo_support STATIC
    src/config/provider_config.cpp
    src/logging/logger.cpp)
target_include_directories(anolis_provider_ezo_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(anolis_provider_ezo_support PUBLIC yaml-cpp::yaml-cpp)
target_compile_definitions(anolis_provider_ezo_support PUBLIC
    ANOLIS_PROVIDER_EZO_VERSION="${PROJECT_VERSION}")
anolis_provider_ezo_apply_warnings(anolis_provider_ezo_support)

add_library(anolis_provider_ezo_core STATIC
    src/core/handlers.cpp
    src/core/health.cpp
    src/core/runtime_state.cpp
    src/core/transport/framed_stdio.cpp
    src/i2c/bus_executor.cpp
    src/i2c/ezo_i2c_bridge.cpp
    src/i2c/session.cpp)
target_include_directories(anolis_provider_ezo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(anolis_provider_ezo_core
    PUBLIC
        adpp_proto
        anolis_provider_ezo_support
        protobuf::libprotobuf
        Threads::Threads
        ezo_core)
target_compile_definitions(anolis_provider_ezo_core PUBLIC
    ANOLIS_PROVIDER_EZO_VERSION="${PROJECT_VERSION}")
anolis_provider_ezo_apply_warnings(anolis_provider_ezo_core)

add_executable(anolis-provider-ezo src/core/main.cpp)
target_include_directories(anolis-provider-ezo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(anolis-provider-ezo PRIVATE anolis_provider_ezo_core)
target_compile_definitions(anolis-provider-ezo PRIVATE
    ANOLIS_PROVIDER_EZO_VERSION="${PROJECT_VERSION}")
anolis_provider_ezo_apply_warnings(anolis-provider-ezo)

if(WIN32)
    target_compile_definitions(anolis_provider_ezo_support PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(anolis_provider_ezo_core PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(anolis-provider-ezo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests/unit)
endif()

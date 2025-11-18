cmake_minimum_required(VERSION 3.10)
project(redisTOON VERSION 0.2.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Find Redis Module SDK
find_path(REDIS_MODULE_INCLUDE_DIR
    NAMES redismodule.h
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/deps/redis
        ENV REDIS_MODULE_SDK
)

if(NOT REDIS_MODULE_INCLUDE_DIR)
    message(STATUS "Redis Module SDK not found, using bundled redismodule.h")
    set(REDIS_MODULE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps)
endif()

# Source files
set(REDISTOON_SOURCES
    src/redistoon.c
    src/toon_encoder.c
    src/toon_decoder.c
    src/toon_memory.c
    src/toon_json.c
    src/toon_path.c
    src/toon_operations.c
)

# Build shared library
add_library(redistoon MODULE ${REDISTOON_SOURCES})

# Include directories
target_include_directories(redistoon PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${REDIS_MODULE_INCLUDE_DIR}
)

# Link options
target_link_libraries(redistoon m)  # Math library

# Set output name
set_target_properties(redistoon PROPERTIES
    PREFIX ""
    OUTPUT_NAME "redistoon"
    SUFFIX ".so"
)

# Installation
install(TARGETS redistoon
    LIBRARY DESTINATION lib/redis/modules
)

# Testing
enable_testing()

# Add test target (if tests are available)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests)
    add_subdirectory(tests OPTIONAL)
endif()

# Benchmarks
if(EXISTS ${CMAKE_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks OPTIONAL)
endif()

# Print configuration
message(STATUS "===========================================")
message(STATUS "redisTOON Configuration")
message(STATUS "===========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Redis Module SDK: ${REDIS_MODULE_INCLUDE_DIR}")
message(STATUS "===========================================")

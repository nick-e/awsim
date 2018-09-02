cmake_minimum_required(VERSION 3.5.1)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# preprocessor defines
set(httpserver_DEFAULT_NUMBER_OF_STATIC_WORKERS 4)
set(httpserver_DEFAULT_DYNAMIC_WORKERS false)

project(httpserver)
add_executable(httpserver
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/Server.cpp)
target_include_directories(httpserver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
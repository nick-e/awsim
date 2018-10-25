cmake_minimum_required(VERSION 3.5.1)
project(awsim)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g -Werror -Wall -Wextra -DAWSIM_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE, "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -Werror -Wall -Wextra")
set(CMAKE_CXX_COMPILER, clang++)

add_library(dynapages SHARED
    ${SRC_DIR}/Client.cpp
    ${SRC_DIR}/HttpRequest.cpp
    ${SRC_DIR}/HttpResponse.cpp)

find_package(Threads)
add_executable(awsimd
    ${SRC_DIR}/awsimd.cpp
    ${SRC_DIR}/Client.cpp
    ${SRC_DIR}/Config.cpp
    ${SRC_DIR}/CriticalException.cpp
    ${SRC_DIR}/Domain.cpp
    ${SRC_DIR}/DynamicPages.cpp
    ${SRC_DIR}/HttpParser.cpp
    ${SRC_DIR}/HttpRequest.cpp
    ${SRC_DIR}/HttpResponse.cpp
    ${SRC_DIR}/ParserDetails.cpp
    ${SRC_DIR}/Resource.cpp
    ${SRC_DIR}/Server.cpp
    ${SRC_DIR}/Worker.cpp
    ${SRC_DIR}/WorkerAndServerFlags.cpp)

target_include_directories(awsimd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_link_libraries(awsimd
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS})

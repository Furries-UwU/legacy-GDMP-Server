cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(GDMP-Server)

set(BUILD_SHARED TRUE)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/external/GDMP-Shared)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/external/GDMP-Shared)

add_subdirectory(external/GDMP-Shared)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.c)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} GDMP-Shared)
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

project(GDMP-Server)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.c src/*.h)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

add_subdirectory(external/GDMP-Shared)

target_link_libraries(${PROJECT_NAME} GDMP-Shared)
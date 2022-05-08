cmake_minimum_required(VERSION 3.12)

project(GDMP_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

add_subdirectory(external/enet)
add_subdirectory(external/fmt)

include_directories(external/enet/include)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.c src/*.h)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} enet fmt)
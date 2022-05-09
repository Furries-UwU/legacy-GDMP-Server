cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

project(GDMP_server)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.c src/*.h)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE external/enet/include)

add_subdirectory(external/enet)
add_subdirectory(external/fmt)

IF (WIN32)
	target_link_libraries(${PROJECT_NAME} winmm wsock32 ws2_32)
ENDIF()

target_link_libraries(${PROJECT_NAME} enet fmt)
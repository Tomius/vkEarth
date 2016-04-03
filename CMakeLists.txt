project(vkEarth C CXX)

cmake_minimum_required(VERSION 2.8.12)

if (NOT CMAKE_VERSION VERSION_LESS "3.0")
    # Until all major package systems have moved to CMake 3,
    # we stick with the older INSTALL_NAME_DIR mechanism
    cmake_policy(SET CMP0042 OLD)
endif()

set(vkEarth_VERSION_MAJOR "1")
set(vkEarth_VERSION_MINOR "0")
set(vkEarth_VERSION_PATCH "0")
set(vkEarth_VERSION_EXTRA "")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Compiler flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ffast-math")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

#--------------------------------------------------------------------
# Add subdirectories
#--------------------------------------------------------------------
add_subdirectory(deps/glfw)
include_directories(SYSTEM deps/glfw/include)

add_subdirectory(deps/glslang)
include_directories(SYSTEM deps/glslang)

add_subdirectory(deps/glm)
include_directories(SYSTEM deps/glm)

include_directories(SYSTEM deps/vkcpp)
include_directories(${vkEarth_SOURCE_DIR}/src/cpp)

include_directories(SYSTEM deps/lodepng)

# This should be the last subdir / include
add_subdirectory(src)

set(CMAKE_MODULE_PATH "${vkEarth_SOURCE_DIR}/deps/glfw/CMake/modules")
find_package(Vulkan REQUIRED)


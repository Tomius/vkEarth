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

set (CMAKE_CXX_FLAGS "-g -std=c++11")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#--------------------------------------------------------------------
# Add subdirectories
#--------------------------------------------------------------------
add_subdirectory(deps/glfw)
add_subdirectory(deps/glslang)
add_subdirectory(src)

set(CMAKE_MODULE_PATH "${vkEarth_SOURCE_DIR}/deps/glfw/CMake/modules")
find_package(Vulkan REQUIRED)


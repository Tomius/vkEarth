cmake_minimum_required(VERSION 2.8)

link_libraries(glfw)
link_libraries(glslang OGLCompiler OSDependent SPIRV)

set (CMAKE_CXX_FLAGS "-g -std=c++11")

if (BUILD_SHARED_LIBS)
    link_libraries("${MATH_LIBRARY}")
endif()

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

file(GLOB vkEarth_SOURCE "cpp/*.cpp")

add_executable(vkEarth WIN32 ${vkEarth_SOURCE} ${ICON})
target_include_directories(vkEarth PRIVATE "${VULKAN_INCLUDE_DIR}")
target_link_libraries(vkEarth "${VULKAN_LIBRARY}")
set(WINDOWS_BINARIES vkEarth)
SET(EXECUTABLE_OUTPUT_PATH ${vkEarth_SOURCE_DIR})

# Copy shader codes
add_custom_command(TARGET vkEarth PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/src/glsl $<TARGET_FILE_DIR:vkEarth>/src/glsl)

if (MSVC)
    # Tell MSVC to use main instead of WinMain for Windows subsystem executables
    set_target_properties(${WINDOWS_BINARIES} PROPERTIES
                          LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

if (APPLE)
    set_target_properties(empty PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Empty Event")
    set_target_properties(sharing PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Sharing")
    set_target_properties(tearing PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Tearing")
    set_target_properties(threads PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Threads")
    set_target_properties(title PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Title")
    set_target_properties(windows PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Windows")

    set_target_properties(${WINDOWS_BINARIES} PROPERTIES
                          MACOSX_BUNDLE_SHORT_VERSION_STRING ${vkEarth_VERSION}
                          MACOSX_BUNDLE_LONG_VERSION_STRING ${vkEarth_VERSION_FULL}
                          MACOSX_BUNDLE_INFO_PLIST "${vkEarth_SOURCE_DIR}/deps/glfw/CMake/MacOSXBundleInfo.plist.in")
endif()


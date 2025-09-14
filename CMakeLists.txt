cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Cambion)

file(GLOB_RECURSE CPP_SOURCE_FILES "src/*.h" "src/*.cpp")

add_executable(Cambion ${CPP_SOURCE_FILES})

set_target_properties(Cambion PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

target_compile_definitions(Cambion PRIVATE GLFW_INCLUDE_VULKAN GLM_FORCE_XYZW_ONLY GLM_FORCE_QUAT_DATA_XYZW GLM_FORCE_QUAT_CTOR_XYZW)
target_include_directories(Cambion PRIVATE dependencies/glm "$ENV{VULKAN_SDK}/Include")
target_link_directories(Cambion PRIVATE "$ENV{VULKAN_SDK}/Lib")

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory(dependencies/glfw)
endif()

target_link_libraries(Cambion PRIVATE glfw vulkan-1)



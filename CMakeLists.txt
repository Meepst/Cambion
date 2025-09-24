cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Cambion)

file(GLOB_RECURSE CPP_SOURCE_FILES "src/*.h" "src/*.cpp")
file(GLOB_RECURSE GLSL_SOURCE_FILES "src/shaders/*.glsl")
file(GLOB_RECURSE GLSL_HEADER_FILES "src/shaders/*.h")

add_executable(Cambion ${CPP_SOURCE_FILES})

set_target_properties(Cambion PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

target_compile_definitions(Cambion PRIVATE GLFW_INCLUDE_VULKAN GLM_FORCE_XYZW_ONLY GLM_FORCE_QUAT_DATA_XYZW GLM_FORCE_QUAT_CTOR_XYZW)
target_include_directories(Cambion PRIVATE dependencies/glm dependencies/fast_obj "$ENV{VULKAN_SDK}/Include")
target_link_directories(Cambion PRIVATE "$ENV{VULKAN_SDK}/Lib")

if(WIN32)
  target_compile_definitions(Cambion PRIVATE GLFW_EXPOSE_NATIVE_WIN32)
  target_compile_definitions(Cambion PRIVATE WIN32_LEAN_and_MEAN NOMINMAX)
endif()

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory(dependencies/glfw)
endif()

target_link_libraries(Cambion PRIVATE glfw vulkan-1)

if(UNIX)
  if(DEFINED ENV{VULKAN_SDK})
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
  else()
    set(GLSL_VALIDATOR "glslangValidator")
  endif()
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()



set(SPIRV_OUTPUT_DIR "${PROJECT_BINARY_DIR}/$<CONFIG>/spirv/")
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(STEM ${GLSL} NAME_WLE)
  set(SPIRV "${SPIRV_OUTPUT_DIR}${STEM}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL_FLAGS} --quiet ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL} ${GLSL_HEADER_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Cambion compile_shaders)
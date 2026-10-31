cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(LMNX_Deko3D LANGUAGES C CXX)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
  message(FATAL_ERROR "LMNX-Deko3D requires the Nintendo Switch toolchain")
endif()
add_subdirectory(third_party/uam)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE _shader_fuentes
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.inc")
list(FILTER _shader_fuentes EXCLUDE REGEX "/referencias/")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_shader_fuentes}
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/shader_build_id.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/shader_build_id.py"
  OUTPUT_VARIABLE _shader_id OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
add_library(lmnx_deko3d STATIC
  third_party/plume/plume_deko3d.cpp
  third_party/plume/plume_deko3d_commands.cpp
  src/backend.cpp src/shader_cache.cpp src/native_gfx.cpp)
set_target_properties(lmnx_deko3d PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(lmnx_deko3d PUBLIC include third_party/plume)
target_link_libraries(lmnx_deko3d PUBLIC uamlib deko3d nx)
target_compile_options(lmnx_deko3d PRIVATE -mcpu=cortex-a57 -ffp-contract=off)
target_compile_definitions(lmnx_deko3d PRIVATE LMNX_SHADER_BUILD_ID="${_shader_id}")
add_library(LMNX::Deko3D ALIAS lmnx_deko3d)

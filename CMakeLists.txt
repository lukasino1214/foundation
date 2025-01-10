cmake_minimum_required(VERSION 3.31)
project(foundation)

include(cmake/warnings.cmake)
include(cmake/static_analysis.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT ("${CMAKE_SIZEOF_VOID_P}" EQUAL 8))
  message(WARNING "The NVTT samples require a 64-bit build.")
  return()
endif()

if(WIN32)
  if(NOT ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v14*"))
    message(WARNING "As of this writing, the NVTT C++ API is only guaranteed to be compatible with MSVC v14x (e.g. v142, Visual Studio 2019) toolsets. You may get unexpected results running the C++ samples, but any C samples should work.")
  endif()
endif()

# Find NVTT.
if(NOT NVTT_DIR)
  set(_SEARCH_PATHS "$ENV{NVTT_DIR}" "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_LIST_DIR}/../nvtt" "C:/Program Files/NVIDIA Corporation/NVIDIA Texture Tools" "D:/Program Files/NVIDIA Corporation/NVIDIA Texture Tools")
  
  foreach(_PATH ${_SEARCH_PATHS})
    file(GLOB _DLL_POSSIBILITIES "${_PATH}/nvtt*.dll" "${_PATH}/libnvtt.so.*")
    if(_DLL_POSSIBILITIES) # If this folder contains a DLL matching the NVTT DLL pattern
      set(NVTT_DIR "${_PATH}")
      break()
    endif()
  endforeach()
  
  if(NOT NVTT_DIR)
    message(WARNING "NVTT not found! Please install NVTT from https://developer.nvidia.com/nvidia-texture-tools-exporter and set the CMake NVTT_DIR variable to the folder containing nvtt*.dll (e.g. C:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools).")
    return()
  endif()
endif()

# Get the NVTT shared library name.
file(GLOB _NVTT_SL_POSSIBILITIES "${NVTT_DIR}/nvtt*.dll" "${NVTT_DIR}/libnvtt.so.*")
if(NOT _NVTT_SL_POSSIBILITIES)
  message(WARNING "NVTT_DIR didn't contain an NVTT shared library of the form nvtt*.dll or libnvtt.so.*! Is NVTT_DIR set correctly? NVTT_DIR was ${NVTT_DIR}")
  return()
else()
  list(LENGTH _NVTT_SL_POSSIBILITIES _NVTT_SL_POSSIBILITIES_LEN)
  math(EXPR _NVTT_SL_IDX ${_NVTT_SL_POSSIBILITIES_LEN}-1)
  list(GET _NVTT_SL_POSSIBILITIES ${_NVTT_SL_IDX} _NVTT_SL)
endif()

# Find the NVTT linker library on Windows.
if(WIN32)
  if(NOT NVTT_LIB)
    file(GLOB _NVTT_LIB_ALL "${NVTT_DIR}/lib/x64-v*/nvtt*.lib")
    if(NOT _NVTT_LIB_ALL)
      message(WARNING "Found nvtt.dll in ${NVTT_DIR}, but was unable to find nvtt.lib in ${NVTT_DIR}/lib/... ! Please check the NVTT directory and this CMake script to see if the path is correct.")
      return()
    endif()
    list(LENGTH _NVTT_LIB_ALL _NVTT_LIB_LEN)
    math(EXPR _NVTT_LIB_IDX ${_NVTT_LIB_LEN}-1)
    list(GET _NVTT_LIB_ALL ${_NVTT_LIB_IDX} NVTT_LIB)
  endif()
endif()

# Print out information to help with error reports.
message(STATUS "NVTT Shared Library: ${_NVTT_SL}")
message(STATUS "NVTT .lib (Windows): ${NVTT_LIB}")

find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(fmt CONFIG REQUIRED)

find_package(daxa CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(metis CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(quill CONFIG REQUIRED)

add_executable(${PROJECT_NAME} 
    "src/main.cpp"
    "src/application.cpp"
    "src/common/thread_pool.cpp"
    "src/ecs/asset_manager.cpp"
    "src/ecs/asset_processor.cpp"
    "src/ecs/components.cpp"
    "src/ecs/entity.cpp"
    "src/ecs/scene.cpp"
    "src/graphics/context.cpp"
    "src/graphics/utils/gpu_metric.cpp"
    "src/graphics/window.cpp"
    "src/graphics/renderer.cpp"
    "src/graphics/helper.cpp"
    "src/graphics/camera.cpp"
    "src/graphics/virtual_geometry/virtual_geometry.cpp"
    "src/math/decompose.cpp"
    "src/scripting/scripting_engine.cpp"
    "src/ui/panels/viewport_panel.cpp"
    "src/ui/panels/scene_hierarchy_panel.cpp"
    "src/ui/panels/file_browser.cpp"
    "src/ui/ui.cpp"
    "src/utils/file_io.cpp"
    "src/utils/zstd.cpp"
    "src/utils/file_watcher.cpp"
    "src/utils/logger.cpp"
)

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.hpp")

set_project_warnings(${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:slang::glslang> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE "src")
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${NVTT_LIB}")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE "${_NVTT_SL}")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE 
    daxa::daxa 
    glfw 
    imgui::imgui
    glm::glm 
    fastgltf::fastgltf 
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    Tracy::TracyClient
    imguizmo::imguizmo
    ${LUA_LIBRARIES}
    sol2
    metis
    meshoptimizer::meshoptimizer
    simdjson::simdjson
    quill::quill
)

target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR} ${LUA_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE "${NVTT_DIR}/include")

if(TRUE)
  target_compile_options(${PROJECT_NAME} PUBLIC -g -gcodeview -fms-hotpatch -ffunction-sections -Xclang -mno-constructor-aliases)
  target_link_options(${PROJECT_NAME} PUBLIC -Xlinker /FUNCTIONPADMIN)
  target_include_directories(${PROJECT_NAME} PRIVATE "deps/LivePP/API/x64")
  target_compile_definitions(${PROJECT_NAME} PUBLIC LPP_PATH="${CMAKE_CURRENT_SOURCE_DIR}/deps/LivePP" LPP_ENABLED)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
endif()

set(COMPILE_COMMANDS_FILE "${CMAKE_BINARY_DIR}/compile_commands.json")
set(DESTINATION_FILE "${CMAKE_SOURCE_DIR}/compile_commands.json")

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${COMPILE_COMMANDS_FILE}"
    "${DESTINATION_FILE}"
    COMMENT "Copying compile_commands.json to root directory"
)

add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${COMPILE_COMMANDS_FILE}"
    "${DESTINATION_FILE}"
    COMMENT "Copying compile_commands.json to root directory"
)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND cmake -E copy_if_different "${_NVTT_SL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

add_dependencies(copy_compile_commands ${PROJECT_NAME})


install(FILES "${_NVTT_SL}" CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug")
install(FILES "${_NVTT_SL}" CONFIGURATIONS Release DESTINATION "bin_${ARCH}")

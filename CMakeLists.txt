cmake_minimum_required(VERSION 3.21)
project(shaper)

include(cmake/warnings.cmake)
include(cmake/static_analysis.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
find_package(Ktx CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

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
)
target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.hpp")

set_project_warnings(${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:slang::glslang> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE "src")
target_link_libraries(${PROJECT_NAME} PRIVATE 
    daxa::daxa 
    glfw 
    imgui::imgui
    glm::glm 
    fastgltf::fastgltf 
    KTX::ktx
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    meshoptimizer::meshoptimizer
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
)
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
endif()

set(COMPILE_COMMANDS_FILE "${CMAKE_BINARY_DIR}/compile_commands.json")
set(DESTINATION_FILE "${CMAKE_SOURCE_DIR}/compile_commands.json")

add_custom_command(
    TARGET shaper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${COMPILE_COMMANDS_FILE}"
    "${DESTINATION_FILE}"
    COMMENT "Copying compile_commands.json to root directory"
)

# Optionally, you can add this command to all targets
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${COMPILE_COMMANDS_FILE}"
    "${DESTINATION_FILE}"
    COMMENT "Copying compile_commands.json to root directory"
)

# Ensure the copy happens after the build
add_dependencies(copy_compile_commands shaper)

cmake_minimum_required (VERSION 3.31)
project(foundation)

include(cmake/warnings.cmake)
include(cmake/static_analysis.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(cmake/deps.cmake)

set(DAXA_ENABLE_UTILS_IMGUI true)
set(DAXA_ENABLE_UTILS_MEM true)
set(DAXA_ENABLE_UTILS_PIPELINE_MANAGER_GLSLANG true)
set(DAXA_ENABLE_UTILS_PIPELINE_MANAGER_SLANG true)
set(DAXA_ENABLE_UTILS_TASK_GRAPH true)
set(DAXA_ENABLE_TESTS false)
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/daxa)

add_executable(${PROJECT_NAME}
    "src/main.cpp"
    "src/application.cpp"
    "src/asset/asset_manager.cpp"
    "src/asset/asset_processor.cpp"
    "src/asset/image_processor.cpp"
    "src/asset/mesh_processor.cpp"
    "src/asset/gltf_loader.cpp"
    "src/asset/fbx_loader.cpp"
    "src/ecs/entity.cpp"
    "src/ecs/scene.cpp"
    "src/graphics/window.cpp"
    "src/graphics/gpu_context.cpp"
    "src/graphics/renderer.cpp"
    "src/graphics/camera.cpp"
    "src/graphics/helper.cpp"
    "src/graphics/gpu_scene.cpp"
    "src/utils/thread_pool.cpp"
    "src/utils/io.cpp"
    "src/math/math.cpp"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(TARGET foundation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${Slang_DLL} $<TARGET_FILE_DIR:foundation>
        COMMAND ${CMAKE_COMMAND} -E copy ${Slang_Glslang_DLL} $<TARGET_FILE_DIR:foundation>
    )
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.hpp")
set_project_warnings(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_26)
target_include_directories(${PROJECT_NAME} PRIVATE "src")

target_link_libraries(${PROJECT_NAME} PRIVATE 
    fmt::fmt
    daxa::daxa
    glfw
    fastgltf::fastgltf
    glm::glm
    unordered_dense::unordered_dense
    dds_image::dds_image
    wuffs::wuffs
    meshoptimizer::meshoptimizer
    flecs::flecs_static
    Tracy::TracyClient
    ufbx::ufbx
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
endif()

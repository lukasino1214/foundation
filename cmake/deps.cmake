find_package(Vulkan REQUIRED)

include(FetchContent)
set(FETCHCONTENT_FULLY_DISCONNECTED OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/daxa/CMakeLists.txt")
    find_package(Git REQUIRED)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()

if (NOT TARGET fmt::fmt)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG        407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(fmt)
endif()

if (NOT TARGET fastgltf::fastgltf)
    FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf
        GIT_TAG        0d1b67a28c4950ea2deb796702006dcbe31e02b3
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(fastgltf)
endif()

if (NOT TARGET glm::glm)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG        8d1fd52e5ab5590e2c81768ace50c72bae28f2ed
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(glm)
endif()

if (NOT TARGET glfw)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        7b6aead9fb88b3623e3b3725ebb42670cbe4c579
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(glfw)
endif()

if (NOT TARGET unordered_dense::unordered_dense)
    FetchContent_Declare(
        unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense
        GIT_TAG        3234af2c03549bc85656bfd3a86993bf1cd8aef1
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(unordered_dense)
endif()

if (NOT TARGET imgui::imgui)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG        fdc084f532189fda8474079f79e74fa5e3541c9f
        GIT_SHALLOW    OFF # docking branch cope
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_MakeAvailable(imgui)

        add_library(lib_imgui
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp)

        target_include_directories(lib_imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${Vulkan_INCLUDE_DIRS})

        if(TARGET glfw)
            target_sources(lib_imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
            target_link_libraries(lib_imgui PRIVATE glfw)
            target_include_directories(lib_imgui PUBLIC ${glfw_SOURCE_DIR}/include)
        endif()

        add_library(imgui::imgui ALIAS lib_imgui)
    endif()
endif()

if(NOT TARGET dds_image::dds_image)
    FetchContent_Declare(
        dds_image
        GIT_REPOSITORY https://github.com/spnda/dds_image
        GIT_TAG        84a7bd47293a87214d454992deb8fdf3cba91c48
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(dds_image)

    add_library(dds_image_lib INTERFACE)
    target_include_directories(dds_image_lib INTERFACE "${dds_image_SOURCE_DIR}/include")

    add_library(dds_image::dds_image ALIAS dds_image_lib)
endif()

if(NOT TARGET wuffs::wuffs)
    FetchContent_Declare(
        wuffs
        GIT_REPOSITORY https://github.com/google/wuffs
        GIT_TAG        072595ae7df55544874caf5096cd9ef4ecf1bdbb
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(wuffs)

    add_library(wuffs_lib INTERFACE)
    target_include_directories(wuffs_lib INTERFACE "${wuffs_SOURCE_DIR}/release/c")

    add_library(wuffs::wuffs ALIAS wuffs_lib)
endif()

if(NOT TARGET meshoptimizer::meshoptimizer)
    FetchContent_Declare(
        meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer
        GIT_TAG        73583c335e541c139821d0de2bf5f12960a04941
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(meshoptimizer)

    add_library(meshoptimizer::meshoptimizer ALIAS meshoptimizer)
endif()

if(NOT TARGET flecs::flecs_static)
    FetchContent_Declare(
        flecs
        GIT_REPOSITORY https://github.com/SanderMertens/flecs
        GIT_TAG        dcbaf278d15379aadea10421bbcbd3c4ead529a3
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(flecs)
endif()

if(NOT TARGET Tracy::TracyClient)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy
        GIT_TAG        05cceee0df3b8d7c6fa87e9638af311dbabc63cb
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(tracy)
endif()
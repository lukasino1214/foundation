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

if (NOT TARGET implot::implot)
    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot
        GIT_TAG        v0.17
        GIT_SHALLOW    ON
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_GetProperties(implot)
    if(NOT implot_POPULATED)
        FetchContent_MakeAvailable(implot)

        add_library(lib_implot
            ${implot_SOURCE_DIR}/implot.cpp
            ${implot_SOURCE_DIR}/implot_items.cpp
            ${implot_SOURCE_DIR}/implot_demo.cpp)

        target_include_directories(lib_implot PUBLIC ${implot_SOURCE_DIR})
        target_link_libraries(lib_implot PRIVATE imgui::imgui)

        add_library(implot::implot ALIAS lib_implot)
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
        GIT_TAG        2560db99aa19c524880b56b0119cd8e979597490
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
        GIT_SHALLOW    OFF
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(tracy)
endif()

if(NOT TARGET ufbx::ufbx)
    FetchContent_Declare(
        ufbx
        GIT_REPOSITORY https://github.com/ufbx/ufbx
        GIT_TAG        83bc7cf44f76bc8622de63b809a42b5d557cd733
        GIT_SHALLOW    ON
        GIT_PROGRESS   TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_GetProperties(ufbx)
    if(NOT ufbx_POPULATED)
        FetchContent_MakeAvailable(ufbx)

        add_library(lib_ufbx ${ufbx_SOURCE_DIR}/ufbx.c)
        target_include_directories(lib_ufbx PUBLIC ${ufbx_SOURCE_DIR})

        if(UNIX AND NOT APPLE)
            target_link_libraries(lib_ufbx PRIVATE m)
        endif()

        add_library(ufbx::ufbx ALIAS lib_ufbx)
    endif()
endif()

if(NOT TARGET bc7enc_rdo::bc7enc_rdo)
    FetchContent_Declare(
        bc7enc_rdo
        GIT_REPOSITORY https://github.com/richgel999/bc7enc_rdo
        GIT_TAG        efca5c04451e2882a8ad9d234115ef49a9418a4f
        GIT_SHALLOW    ON
        GIT_PROGRESS   TRUE
        EXCLUDE_FROM_ALL
    )

    option(SUPPORT_BC7E "Enable BC7E ISPC encoder in bc7enc_rdo" ON)

    FetchContent_GetProperties(bc7enc_rdo)
    if(NOT bc7enc_rdo_POPULATED)
        FetchContent_MakeAvailable(bc7enc_rdo)

       add_library(bc7enc_rdo_lib STATIC
            ${bc7enc_rdo_SOURCE_DIR}/bc7enc.cpp
            ${bc7enc_rdo_SOURCE_DIR}/rgbcx.cpp
            ${bc7enc_rdo_SOURCE_DIR}/bc7decomp.cpp
            ${bc7enc_rdo_SOURCE_DIR}/ert.cpp
        )

        target_include_directories(bc7enc_rdo_lib SYSTEM PUBLIC
            ${bc7enc_rdo_SOURCE_DIR}
        )

        find_package(OpenMP)
        if(OpenMP_CXX_FOUND)
            target_link_libraries(bc7enc_rdo_lib INTERFACE OpenMP::OpenMP_CXX)
        endif()

        add_library(bc7enc_rdo::bc7enc_rdo ALIAS bc7enc_rdo_lib)
    endif()
endif()
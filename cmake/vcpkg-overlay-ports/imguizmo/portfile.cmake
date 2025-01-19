vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CedricGuillemet/ImGuizmo
    REF b10e91756d32395f5c1fefd417899b657ed7cb88
    SHA512 242bf8fc7e63b3d586bba622ba73c37a57a2143032c2f4db23f35c97a5a9625b7c0e12ee27b1816360a00c6fd461f0bd9098ad5b50cf36c335081a789843c2fc
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_DEBUG
        -DIMGUIZMO_SKIP_HEADERS=ON
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH share/${PORT})

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

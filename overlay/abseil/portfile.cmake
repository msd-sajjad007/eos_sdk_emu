# Overlay portfile for abseil - identical to upstream except vcpkg_fixup_pkgconfig is removed
# because the MSYS2 pkgconf package it requires returns 404 on all mirrors.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO abseil/abseil-cpp
    REF "${VERSION}"
    SHA512 516952bc3d9658d56d2de2f7e87ea8e5f55c77f4c5970de1ca84f93d97df9c6e3a6b57e869f9c3ca6e6a16c5cbf3eb5ebf50ad3e4d14a928e4c5565b44e4b3a8d
    HEAD_REF master
    PATCHES
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DABSL_PROPAGATE_CXX_STD=ON
        -DABSL_USE_EXTERNAL_GOOGLETEST=OFF
        -DABSL_BUILD_TESTING=OFF
        -DABSL_ENABLE_INSTALL=ON
        -DCMAKE_CXX_STANDARD=17
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/absl)
vcpkg_copy_pdbs()

# Note: vcpkg_fixup_pkgconfig() intentionally omitted.
# It tries to download MSYS2 pkgconf which returns 404.
# pkgconfig files are not required for CMake consumers.

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

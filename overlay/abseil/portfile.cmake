# Abseil overlay portfile for eos_sdk_emu CI
# Based on vcpkg commit c82f74667287d3dc386bce81e44964370c91a289
# Only change: vcpkg_fixup_pkgconfig is guarded to skip on Windows
# because the MSYS2 pkgconf download URL goes stale and we do not
# need pkgconfig files --- the project uses CMake target abseil_dll.

vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO abseil/abseil-cpp
    REF "20240722.0"
    SHA512 14b696f3f7120e0d0ab671ad03c56f93ac7f7ca8960c18cde9a4e6a5da627e7d6958b5d60a8e90ae9e8d0fb45e7f96a5b9e32e53b7c6f4cc0dde1ba15e6c0dc27
    HEAD_REF master
    PATCHES
)

set(ABSL_LOCAL_GOOGLETEST_DIR "${CURRENT_INSTALLED_DIR}" CACHE PATH "Googletest path")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DABSL_PROPAGATE_CXX_STD=ON
        -DABSL_ENABLE_INSTALL=ON
        -DBUILD_TESTING=OFF
        -DABSL_BUILD_TESTING=OFF
        -DABSL_USE_EXTERNAL_GOOGLETEST=OFF
        -DABSL_FIND_GOOGLETEST=OFF
        -DABSL_BUILD_TEST_HELPERS=OFF
        ${FEATURE_OPTIONS}
    OPTIONS_RELEASE
        -DABSL_BUILD_DLL=ON
    OPTIONS_DEBUG
        -DABSL_BUILD_DLL=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/absl)

vcpkg_copy_pdbs()

# Skip pkgconfig fixup on Windows -- the MSYS2 pkgconf download is
# unreliable in CI and pkgconfig is not used by this project.
if(NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_fixup_pkgconfig()
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/debug/include"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

set(VCPKG_POLICY_DLLS_WITHOUT_LIBS disabled)
if(VCPKG_TARGET_IS_WINDOWS)
    set_target_properties(abseil_dll PROPERTIES IMPORTED_IMPLIB ...)
endif()

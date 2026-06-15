vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO protocolbuffers/protobuf
    REF "v${VERSION}"
    SHA512 ce81add9d978a6b63d4205715eac5084e81a6753da1f6c6bad6493e60253215901bffc4a60d704a873333f2b9f94fd86cb7eb5b293035f2268c12692bd808bac
    HEAD_REF master
    PATCHES
        fix-static-build.patch
        fix-default-proto-file-path.patch
        fix-arm64-msvc.patch
        # fix-utf8-range.patch intentionally removed:
        # utf8_range is a standalone overlay port (no-op shim); protobuf
        # builds and installs utf8_range from its own bundled source.
)

string(COMPARE EQUAL "${TARGET_TRIPLET}" "${HOST_TRIPLET}" protobuf_BUILD_PROTOC_BINARIES)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" protobuf_BUILD_SHARED_LIBS)
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" protobuf_MSVC_STATIC_RUNTIME)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        zlib protobuf_WITH_ZLIB
)

if(VCPKG_TARGET_IS_UWP)
    set(protobuf_BUILD_LIBPROTOC OFF)
else()
    set(protobuf_BUILD_LIBPROTOC ON)
endif()

# Delete language backends we aren't targeting to reduce false positives in automated dependency
# detectors like Dependabot.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/csharp"
    "${SOURCE_PATH}/java"
    "${SOURCE_PATH}/lua"
    "${SOURCE_PATH}/objectivec"
    "${SOURCE_PATH}/php"
    "${SOURCE_PATH}/python"
    "${SOURCE_PATH}/ruby"
    "${SOURCE_PATH}/rust"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -Dprotobuf_BUILD_SHARED_LIBS=${protobuf_BUILD_SHARED_LIBS}
        -Dprotobuf_MSVC_STATIC_RUNTIME=${protobuf_MSVC_STATIC_RUNTIME}
        -Dprotobuf_BUILD_TESTS=OFF
        -DCMAKE_INSTALL_CMAKEDIR:STRING=share/protobuf
        -Dprotobuf_BUILD_PROTOC_BINARIES=${protobuf_BUILD_PROTOC_BINARIES}
        -Dprotobuf_BUILD_LIBPROTOC=${protobuf_BUILD_LIBPROTOC}
        -Dprotobuf_ABSL_PROVIDER=package
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

if(protobuf_BUILD_PROTOC_BINARIES)
    if(VCPKG_TARGET_IS_WINDOWS)
        vcpkg_copy_tools(TOOL_NAMES protoc AUTO_CLEAN)
    else()
        string(REPLACE "." ";" VERSION_LIST ${VERSION})
        list(GET VERSION_LIST 1 VERSION_MINOR)
        list(GET VERSION_LIST 2 VERSION_PATCH)
        vcpkg_copy_tools(TOOL_NAMES protoc protoc-${VERSION_MINOR}.${VERSION_PATCH}.0 AUTO_CLEAN)
    endif()
else()
    file(COPY "${CURRENT_HOST_INSTALLED_DIR}/tools/${PORT}" DESTINATION "${CURRENT_PACKAGES_DIR}/tools")
endif()

vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/${PORT}/protobuf-config.cmake"
    "if(protobuf_MODULE_COMPATIBLE)"
    "if(1)"
)
if(NOT protobuf_BUILD_LIBPROTOC)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/${PORT}/protobuf-module.cmake"
        "_protobuf_find_libraries(Protobuf_PROTOC protoc)"
        ""
    )
endif()

vcpkg_cmake_config_fixup()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/google/protobuf/port_def.inc"
        "\#ifdef PROTOBUF_PORT_"
        "\#ifndef PROTOBUF_USE_DLLS\n\#define PROTOBUF_USE_DLLS\n\#endif // PROTOBUF_USE_DLLS\n\n\#ifdef PROTOBUF_PORT_"
    )
endif()

# vcpkg_copy_pdbs() removed — not needed for static-only builds
# vcpkg_fixup_pkgconfig() removed — MSYS2 pkgconf download returns 404 in CI

if(NOT protobuf_BUILD_PROTOC_BINARIES)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/protobuf-targets-vcpkg-protoc.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/protobuf-targets-vcpkg-protoc.cmake" COPYONLY)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/vcpkg-cmake-wrapper.cmake" @ONLY)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share" "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

#CMAKE_TOOLCHAIN_FILE

project(nemirtingas_epic_emu)
cmake_minimum_required(VERSION 3.0)

# Workaround because cross-compiling with CMake + vcpkg fucks up this variable
if(X86 AND NOT X64)
  set(CMAKE_SIZEOF_VOID_P "4")
elseif(X64 AND NOT X86)
  set(CMAKE_SIZEOF_VOID_P "8")
else()
  message(FATAL_ERROR "Please define either -DX86=ON or -DX64=ON")
endif()

if(WIN32)
  if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
  endif()

  if(X64)
    set(EPIC_API EOSSDK-Win64-Shipping)
    set(OUT_DIR win64)
  elseif(X86)
    set(EPIC_API EOSSDK-Win32-Shipping)
    set(OUT_DIR win32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()

elseif(APPLE)
  message(STATUS "CMake for APPLE is experimental")
  if(X64)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(OUT_DIR macosx64)
  elseif(X86)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(OUT_DIR macosx32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
  set(EPIC_API libEOSSDK-Mac-Shipping)

elseif(UNIX)
  if(X64)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(OUT_DIR linux64)
  elseif(X86)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(OUT_DIR linux32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()
  set(EPIC_API libEOSSDK-Linux-Shipping)

else()
  message(FATAL_ERROR "No CMake for other platforms")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Debug or Release")

option(DISABLE_LOG "Disable all logging" OFF)
option(USE_ZSTD_COMPRESS "Use zstd to compress network messages" OFF)

set(Protobuf_USE_STATIC_LIBS ON)
include(FindProtobuf)
find_package(Protobuf CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(utf8_range CONFIG QUIET)

if(USE_ZSTD_COMPRESS)
  find_package(zstd CONFIG REQUIRED)
endif()

########################################
## net.h net.cc
protobuf_generate_cpp(net_PROTO_SRCS net_PROTO_HDRS proto/network_proto.proto)

########################################
file(
  GLOB
  emu_sources
  eos_dll/*.cpp
  managers/*.cpp
)

if(WIN32)
  file(
    GLOB
    detours_sources
    extra/detours/*.cpp
  )
endif()

file(
  GLOB
  socket_sources
  extra/Socket/src/common/*.cpp
  extra/Socket/src/ipv4/*.cpp
)

file(
  GLOB
  utils_sources
  extra/utils/src/*.cpp
)

file(
  GLOB
  mini_detour_sources
  extra/mini_detour/*.cpp
)

########################################
add_library(
  ${EPIC_API}
  SHARED
  ${emu_sources}
  ${net_PROTO_SRCS}
  ${mini_detour_sources}
  ${socket_sources}
  ${utils_sources}
)

if(UNIX)
  SET_TARGET_PROPERTIES(${EPIC_API} PROPERTIES PREFIX "")
endif()

# utf8_range targets — provided by protobuf's own install tree (our shim is empty).
# Guard with TARGET checks so this works whether or not the shim exported anything.
set(_UTF8_RANGE_LIBS "")
if(TARGET utf8_range::utf8_range)
  list(APPEND _UTF8_RANGE_LIBS utf8_range::utf8_range)
endif()
if(TARGET utf8_range::utf8_validity)
  list(APPEND _UTF8_RANGE_LIBS utf8_range::utf8_validity)
endif()

target_link_libraries(
  ${EPIC_API}
  # Full protobuf (not lite) so all reflection / descriptor symbols resolve.
  # The imported target carries transitive absl + utf8_range deps automatically
  # when linking against the DLL triplet; for the static triplet we also add
  # the individual absl targets below.
  protobuf::libprotobuf
  ${_UTF8_RANGE_LIBS}
  Threads::Threads
  $<$<BOOL:${USE_ZSTD_COMPRESS}>:libzstd>

  # abseil — list every component that protobuf 4.25.x pulls in so the
  # static build resolves all symbols without relying on INTERFACE deps.
  absl::base
  absl::strings
  absl::str_format
  absl::synchronization
  absl::status
  absl::statusor
  absl::flat_hash_map
  absl::flat_hash_set
  absl::log
  absl::log_internal_check_op
  absl::log_internal_conditions
  absl::log_internal_message
  absl::log_internal_nullguard
  absl::log_initialize
  absl::log_globals
  absl::log_sink
  absl::log_sink_registry
  absl::log_entry
  absl::log_severity
  absl::raw_logging_internal
  absl::spinlock_wait
  absl::malloc_internal
  absl::time
  absl::time_zone
  absl::int128
  absl::cord
  absl::cord_internal
  absl::crc32c
  absl::crc_cord_state
  absl::hash
  absl::city
  absl::bad_optional_access
  absl::bad_variant_access

  # Windows system libs
  $<$<BOOL:${WIN32}>:ws2_32>
  $<$<BOOL:${WIN32}>:iphlpapi>
  $<$<BOOL:${WIN32}>:shell32>
  $<$<BOOL:${WIN32}>:user32>

  # UNIX
  $<$<BOOL:${UNIX}>:dl>
)

target_include_directories(
  ${EPIC_API}
  PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  eos_dll/
  managers/
  extra/
  extra/eos_sdk/
  extra/mini_detour/
  extra/utils/include
  extra/Socket/include
)

target_compile_options(
  ${EPIC_API}
  PUBLIC
  $<$<BOOL:${UNIX}>:-fPIC -fpermissive -fvisibility=hidden -Wl,--exclude-libs,ALL>
  $<$<AND:$<BOOL:${UNIX}>,$<BOOL:${X86}>>:-m32>
  $<$<AND:$<BOOL:${UNIX}>,$<BOOL:${X64}>>:-m64>
  $<$<AND:$<CONFIG:>,$<BOOL:${MSVC}>>:/MP>
  $<$<AND:$<CONFIG:Debug>,$<BOOL:${MSVC}>>:/MP>
  $<$<AND:$<CONFIG:Release>,$<BOOL:${MSVC}>>:/MP>
)

target_compile_definitions(
  ${EPIC_API}
  PUBLIC
  __EXPORT_SYMBOLS__
  EPIC_SHARED_LIBRARY
  EPIC_EXPORT
  $<$<BOOL:${USE_ZSTD_COMPRESS}>:NETWORK_COMPRESS>
  $<$<BOOL:${UNIX}>:GNUC>
  $<$<BOOL:${DISABLE_LOG}>:DISABLE_LOG>
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},Release>:EMU_RELEASE_BUILD NDEBUG>
)

##################
## Install rules
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

if(WIN32)
  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR CICD_DEBUG)
    install(TARGETS ${EPIC_API} RUNTIME DESTINATION debug/${OUT_DIR})
  else()
    install(TARGETS ${EPIC_API} RUNTIME DESTINATION release/${OUT_DIR})
  endif()
elseif(APPLE OR UNIX)
  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(TARGETS ${EPIC_API} LIBRARY DESTINATION debug/${OUT_DIR})
  else()
    install(TARGETS ${EPIC_API} LIBRARY DESTINATION release/${OUT_DIR})
  endif()
endif()

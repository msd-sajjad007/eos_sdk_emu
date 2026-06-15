cmake_minimum_required(VERSION 3.15)

# Must be set BEFORE project() so MSVC runtime flags work correctly
cmake_policy(SET CMP0003 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0091 NEW)

project(nemirtingas_epic_emu VERSION 1.0.0 LANGUAGES C CXX)

# ---- Arch guard -------------------------------------------------------
if(X86 AND NOT X64)
  set(CMAKE_SIZEOF_VOID_P 4)
elseif(X64 AND NOT X86)
  set(CMAKE_SIZEOF_VOID_P 8)
else()
  message(FATAL_ERROR "Define exactly one of -DX86=ON or -DX64=ON")
endif()

# ---- Platform setup ---------------------------------------------------
if(WIN32)
  if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Force static CRT (/MT) — must match x64-windows-static triplet
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  if(X64)
    set(EPIC_API EOSSDK-Win64-Shipping)
    set(OUT_DIR  win64)
  else()
    set(EPIC_API EOSSDK-Win32-Shipping)
    set(OUT_DIR  win32)
  endif()

elseif(APPLE)
  message(STATUS "CMake for APPLE is experimental")
  set(arch_flag $<IF:$<BOOL:${X64}>,-m64,-m32>)
  foreach(v CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS)
    string(APPEND ${v} " ${arch_flag}")
  endforeach()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
  set(EPIC_API libEOSSDK-Mac-Shipping)
  set(OUT_DIR  $<IF:$<BOOL:${X64}>,macosx64,macosx32>)

elseif(UNIX)
  set(arch_flag $<IF:$<BOOL:${X64}>,-m64,-m32>)
  foreach(v CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS)
    string(APPEND ${v} " ${arch_flag}")
  endforeach()
  set(EPIC_API libEOSSDK-Linux-Shipping)
  set(OUT_DIR  $<IF:$<BOOL:${X64}>,linux64,linux32>)

else()
  message(FATAL_ERROR "Unsupported platform")
endif()

# ---- C++ standard -----------------------------------------------------
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---- Output dirs ------------------------------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Debug or Release")

# ---- Options ----------------------------------------------------------
option(DISABLE_LOG        "Disable all logging"              OFF)
option(USE_ZSTD_COMPRESS  "Compress network messages w/ zstd" OFF)

# ---- Dependencies (vcpkg manifest drives installation) ---------------
# NOTE: Do NOT call include(FindProtobuf) — it uses legacy module mode
#       and will find the wrong triplet. CONFIG mode is required.
set(Protobuf_USE_STATIC_LIBS ON)
find_package(protobuf   CONFIG REQUIRED)
find_package(Threads    REQUIRED)
find_package(absl       CONFIG REQUIRED)
find_package(utf8_range CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
if(USE_ZSTD_COMPRESS)
  find_package(zstd CONFIG REQUIRED)
endif()

# ---- Protobuf codegen ------------------------------------------------
protobuf_generate_cpp(net_PROTO_SRCS net_PROTO_HDRS proto/network_proto.proto)

# ---- Sources ----------------------------------------------------------
file(GLOB emu_sources         eos_dll/*.cpp managers/*.cpp)
file(GLOB socket_sources      extra/Socket/src/common/*.cpp extra/Socket/src/ipv4/*.cpp)
file(GLOB utils_sources       extra/utils/src/*.cpp)
file(GLOB mini_detour_sources extra/mini_detour/*.cpp)
if(WIN32)
  file(GLOB detours_sources   extra/detours/*.cpp)
endif()

# ---- Library target --------------------------------------------------
add_library(
  ${EPIC_API} SHARED
  ${emu_sources}
  ${net_PROTO_SRCS}
  ${mini_detour_sources}
  ${socket_sources}
  ${utils_sources}
  $<$<BOOL:${WIN32}>:${detours_sources}>
)

if(UNIX)
  set_target_properties(${EPIC_API} PROPERTIES PREFIX "")
endif()

# ---- Link libraries --------------------------------------------------
target_link_libraries(
  ${EPIC_API} PRIVATE

  # Protobuf (lite is enough; pulls utf8_range + absl transitively via CONFIG)
  protobuf::libprotobuf-lite
  Threads::Threads
  nlohmann_json::nlohmann_json

  # utf8_range - required by protobuf v4/v5
  utf8_range::utf8_validity
  utf8_range::utf8_range

  # Abseil complete static set required by protobuf v6
  absl::base
  absl::city
  absl::civil_time
  absl::cord
  absl::cord_internal
  absl::cordz_functions
  absl::cordz_handle
  absl::cordz_info
  absl::cordz_sample_token
  absl::crc32c
  absl::crc_cord_state
  absl::crc_internal
  absl::debugging_internal
  absl::demangle_internal
  absl::die_if_null
  absl::examine_stack
  absl::exponential_biased
  absl::failure_signal_handler
  absl::flags
  absl::flags_commandlineflag
  absl::flags_commandlineflag_internal
  absl::flags_config
  absl::flags_internal
  absl::flags_marshalling
  absl::flags_parse
  absl::flags_private_handle_accessor
  absl::flags_program_name
  absl::flags_reflection
  absl::flags_usage
  absl::flags_usage_internal
  absl::graphcycles_internal
  absl::hash
  absl::hashtablez_sampler
  absl::int128
  absl::kernel_timeout_internal
  absl::leak_check
  absl::log_entry
  absl::log_flags
  absl::log_globals
  absl::log_initialize
  absl::log_internal_check_op
  absl::log_internal_conditions
  absl::log_internal_format
  absl::log_internal_globals
  absl::log_internal_log_sink_set
  absl::log_internal_message
  absl::log_internal_nullguard
  absl::log_internal_proto
  absl::log_severity
  absl::log_sink
  absl::log_sink_registry
  absl::low_level_hash
  absl::malloc_internal
  absl::periodic_sampler
  absl::random_distributions
  absl::random_internal_distribution_test_util
  absl::random_internal_fast_uniform_bits
  absl::random_internal_fastmath
  absl::random_internal_generate_real
  absl::random_internal_iostream_state_saver
  absl::random_internal_mock_helpers
  absl::random_internal_nonsecure_base
  absl::random_internal_pcg_engine
  absl::random_internal_platform
  absl::random_internal_pool_urbg
  absl::random_internal_randen
  absl::random_internal_randen_hwaes
  absl::random_internal_randen_hwaes_impl
  absl::random_internal_randen_slow
  absl::random_internal_salted_seed_seq
  absl::random_internal_seed_material
  absl::random_internal_uniform_helper
  absl::random_internal_wide_multiply
  absl::random_seed_gen_exception
  absl::random_seed_sequences
  absl::raw_hash_set
  absl::raw_logging_internal
  absl::scoped_set_env
  absl::spinlock_wait
  absl::stacktrace
  absl::status
  absl::statusor
  absl::str_format_internal
  absl::strerror
  absl::strings
  absl::strings_internal
  absl::symbolize
  absl::synchronization
  absl::throw_delegate
  absl::time
  absl::time_zone
  absl::vlog_config_internal

  $<$<BOOL:${USE_ZSTD_COMPRESS}>:libzstd>
  $<$<BOOL:${WIN32}>:ws2_32>
  $<$<BOOL:${WIN32}>:iphlpapi>
  $<$<BOOL:${WIN32}>:shell32>
  $<$<BOOL:${WIN32}>:user32>
  $<$<BOOL:${UNIX}>:dl>
)

# ---- Include directories ---------------------------------------------
target_include_directories(
  ${EPIC_API} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  eos_dll/
  managers/
  extra/
  extra/eos_sdk/
  extra/mini_detour/
  extra/utils/include
  extra/Socket/include
)

# ---- Compile options -------------------------------------------------
target_compile_options(
  ${EPIC_API} PUBLIC
  $<$<BOOL:${UNIX}>:-fPIC -fpermissive -fvisibility=hidden -Wl,--exclude-libs,ALL>
  $<$<AND:$<BOOL:${UNIX}>,$<BOOL:${X86}>>:-m32>
  $<$<AND:$<BOOL:${UNIX}>,$<BOOL:${X64}>>:-m64>
  $<$<AND:$<CONFIG:Debug>,$<BOOL:${MSVC}>>:/MP>
  $<$<AND:$<CONFIG:Release>,$<BOOL:${MSVC}>>:/MP>
)

# ---- Compile definitions ---------------------------------------------
target_compile_definitions(
  ${EPIC_API} PUBLIC
  __EXPORT_SYMBOLS__
  EPIC_SHARED_LIBRARY
  EPIC_EXPORT
  $<$<BOOL:${USE_ZSTD_COMPRESS}>:NETWORK_COMPRESS>
  $<$<BOOL:${UNIX}>:GNUC>
  $<$<BOOL:${DISABLE_LOG}>:DISABLE_LOG>
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},Release>:EMU_RELEASE_BUILD NDEBUG>
)

# ---- Install rules ---------------------------------------------------
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

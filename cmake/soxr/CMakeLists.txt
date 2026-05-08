# mostly just the soxr cmakelists but without installation
project (soxr C)
cmake_dependent_option (WITH_VR32
  "Include HQ variable-rate resampling engine." ON
  "WITH_CR32 OR WITH_CR64 OR WITH_CR32S OR WITH_CR64S OR NOT DEFINED WITH_VR32" ON)
cmake_dependent_option (WITH_CR32
  "Include HQ constant-rate resampling engine." ON
  "WITH_VR32 OR WITH_CR64 OR WITH_CR32S OR WITH_CR64S" ON)
cmake_dependent_option (WITH_CR64
  "Include VHQ constant-rate resampling engine." ON
  "WITH_VR32 OR WITH_CR32 OR WITH_CR32S OR WITH_CR64S" ON)
cmake_dependent_option (WITH_CR64S
  "Include VHQ SIMD constant-rate resampling engine." ON
  "WITH_VR32 OR WITH_CR32 OR WITH_CR32S OR WITH_CR64" ON)
cmake_dependent_option (WITH_CR32S
  "Include HQ SIMD constant-rate resampling engine." ON
  "WITH_VR32 OR WITH_CR64 OR WITH_CR32 OR WITH_CR64S" ON)
cmake_dependent_option (WITH_PFFFT
  "Use PFFFT (BSD-like licence) for HQ SIMD DFT." ON
  "WITH_CR32S;NOT WITH_AVFFT" OFF)
cmake_dependent_option (WITH_AVFFT
  "Use libavcodec (LGPL) for HQ SIMD DFT." OFF
  "WITH_CR32S;NOT WITH_PFFFT" OFF)
configure_file (../../lib/soxr/${PROJECT_NAME}-config.h.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.h)

include_directories (${PROJECT_BINARY_DIR})

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/lib/soxr/cmake/Modules/)
message("${CMAKE_MODULE_PATH}")
include (CheckFunctionExists)
include (CheckIncludeFiles)
include (CheckLibraryExists)
include (SetSystemProcessor)
include (TestBigEndian)

set_system_processor ()

check_library_exists (m pow "" NEED_LIBM)
if (NEED_LIBM)
  set (CMAKE_REQUIRED_LIBRARIES "m;${CMAKE_REQUIRED_LIBRARIES}")
  set (LIBM_LIBRARIES m)
endif ()

if (${BUILD_EXAMPLES})
  project (${PROJECT_NAME}) # Adds c++ compiler
endif ()

if (WITH_OPENMP)
  find_package (OpenMP)
  if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    if (MINGW) # Is this still needed?
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_C_FLAGS}")
      set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OpenMP_C_FLAGS}")
    endif ()
  endif()
endif ()

if (WITH_CR32S)
  find_package (SIMD32)
  set (WITH_CR32S ${SIMD32_FOUND})
endif ()

if (WITH_CR64S)
  find_package (SIMD64)
  set (WITH_CR64S ${SIMD64_FOUND})
endif ()

if (WITH_AVFFT)
  find_package (LibAVCodec REQUIRED)
  if (AVCODEC_FOUND)
    include_directories (${AVCODEC_INCLUDE_DIRS})
    set (LIBS ${LIBS} ${AVCODEC_LIBRARIES})
  endif ()
endif ()

if (WITH_AVFFT OR (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" AND SIMD32_FOUND AND WITH_CR32))
  find_package (LibAVUtil)
  if (AVUTIL_FOUND)
    include_directories (${AVUTIL_INCLUDE_DIRS})
    set (LIBS ${LIBS} ${AVUTIL_LIBRARIES})
  endif ()
endif ()

check_function_exists (lrint HAVE_LRINT)
check_include_files (fenv.h HAVE_FENV_H)
check_include_files (stdbool.h HAVE_STDBOOL_H)
check_include_files (stdint.h HAVE_STDINT_H)
test_big_endian (HAVE_BIGENDIAN)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set (PROJECT_CXX_FLAGS "${PROJECT_CXX_FLAGS} -Wconversion -Wall -Wextra \
      -pedantic -Wundef -Wpointer-arith -Wno-long-long")
  if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set (PROJECT_CXX_FLAGS "${PROJECT_CXX_FLAGS} -Wno-keyword-macro")
  endif ()
  if (WITH_DEV_GPROF)
    set (PROJECT_CXX_FLAGS "${PROJECT_CXX_FLAGS} -pg")
  endif ()
  # Can use std=c89, but gnu89 should give faster sinf, cosf, etc.:
  set (PROJECT_C_FLAGS "${PROJECT_CXX_FLAGS} \
       -std=gnu89 -Wnested-externs -Wmissing-prototypes -Wstrict-prototypes")
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s") # strip
  endif ()
  cmake_dependent_option (VISIBILITY_HIDDEN
    "Build shared libraries with -fvisibility=hidden." ON
    "BUILD_SHARED_LIBS" OFF)
  mark_as_advanced (VISIBILITY_HIDDEN)
  if (VISIBILITY_HIDDEN)
    add_definitions (-fvisibility=hidden -DSOXR_VISIBILITY)
  endif ()
endif ()

add_subdirectory(../../lib/soxr/src/ ${CMAKE_BINARY_DIR}/lib/soxr/)
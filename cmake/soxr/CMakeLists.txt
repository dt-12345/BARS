project(soxr C)
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
configure_file(../../lib/soxr/${PROJECT_NAME}-config.h.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.h)
include_directories(${PROJECT_BINARY_DIR})
add_subdirectory(../../lib/soxr/src/ ${CMAKE_BINARY_DIR}/lib/soxr/)
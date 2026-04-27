CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

project(BARS CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-g3 -march=native)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

set(OPUS_BUILD_SHARED_LIBRARY OFF)
set(OPUS_BUILD_TESTING OFF)
set(OPUS_BUILD_PROGRAMS OFF)
add_subdirectory(lib/libopus/)

set(MINIAUDIO_INSTALL OFF)
add_subdirectory(lib/miniaudio/)

add_subdirectory(lib/dsptool/)

add_subdirectory(cmake/soxr/)

add_subdirectory(lib/mmh3/)

add_executable(bars
    src/common/align.hpp
    src/common/bitflag.hpp
    src/common/endian.hpp
    src/common/hash.hpp
    src/common/hash.cpp
    src/common/reader.hpp
    src/common/reader.cpp

    src/decode/adpcmDecoder.hpp
    src/decode/adpcmDecoder.cpp
    src/decode/amtaReader.hpp
    src/decode/amtaReader.cpp
    src/decode/barsReader.hpp
    src/decode/barsReader.cpp
    src/decode/bwavReader.hpp
    src/decode/bwavReader.cpp
    src/decode/decoder.hpp
    src/decode/decoder.cpp
    src/decode/minfReader.hpp
    src/decode/minfReader.cpp
    src/decode/opusDecoder.hpp
    src/decode/opusDecoder.cpp
    src/decode/pcmDecoder.hpp
    src/decode/pcmDecoder.cpp
    src/decode/wavReader.hpp
    src/decode/wavReader.cpp

    src/encode/adpcmEncoder.hpp
    src/encode/adpcmEncoder.cpp
    src/encode/amtaWriter.hpp
    src/encode/amtaWriter.cpp
    src/encode/barsWriter.hpp
    src/encode/barsWriter.cpp
    src/encode/bwavWriter.hpp
    src/encode/bwavWriter.cpp
    src/encode/encoder.hpp
    src/encode/encoder.cpp
    src/encode/minfWriter.hpp
    src/encode/minfWriter.cpp
    src/encode/opusEncoder.hpp
    src/encode/opusEncoder.cpp
    src/encode/pcmEncoder.hpp
    src/encode/pcmEncoder.cpp

    src/playback/source.hpp
    src/playback/source.cpp

    src/resource/adpcm.hpp
    src/resource/amta.hpp
    src/resource/bars.hpp
    src/resource/bwav.hpp
    src/resource/minf.hpp
    src/resource/opus.hpp

    src/sound/archive.hpp
    src/sound/archive.cpp
    src/sound/asset.hpp
    src/sound/asset.cpp
    src/sound/channel.hpp
    src/sound/channel.cpp
    src/sound/metadata.hpp
    src/sound/metadata.cpp
    src/sound/music.hpp
    src/sound/music.cpp
    src/sound/sound.hpp
    src/sound/sound.cpp
    src/sound/types.hpp

    src/main.cpp
)

if (MSVC)
    target_compile_options(bars PRIVATE /W4 /wd4244 /wd4127 /Zc:__cplusplus)
else()
    target_compile_options(bars PRIVATE -Wall -Wextra -fno-plt)
endif()

if (MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_link_libraries(bars PRIVATE stdc++exp)
endif()

target_include_directories(bars PRIVATE lib/dsptool/)
target_include_directories(bars PRIVATE lib/libopus/include/)
target_include_directories(bars PRIVATE lib/miniaudio/)
target_include_directories(bars PRIVATE lib/mmh3/)
target_include_directories(bars PRIVATE lib/soxr/src/)
target_include_directories(bars PUBLIC src/)

target_link_libraries(bars PRIVATE dsptool)
target_link_libraries(bars PRIVATE miniaudio)
target_link_libraries(bars PRIVATE mmh3)
target_link_libraries(bars PRIVATE opus)
target_link_libraries(bars PRIVATE soxr)
#pragma once

#include <cstdint>
#include <string_view>

namespace common {

auto CalcCRC32(const std::uint8_t* data, size_t size, std::uint32_t seed = 0xffff'ffffu) -> std::uint32_t;
auto CalcCRC32(const char* str) -> std::uint32_t;
auto CalcCRC32(const std::string_view str) -> std::uint32_t;

struct CRC32Context {
    std::uint32_t value = 0xffff'ffffu;

    auto update(const std::uint8_t* data, size_t size) -> void;
    [[nodiscard]] auto get() const -> std::uint32_t { return ~value; }

    operator std::uint32_t() { return get(); }
};

auto CalcMmh3(const std::uint8_t* data, size_t size, std::uint32_t seed = 0u) -> std::uint32_t;
auto CalcMmh3(const char* str) -> std::uint32_t;
auto CalcMmh3(const std::string_view str) -> std::uint32_t;

} // namespace common
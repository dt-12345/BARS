#pragma once

#include "common/align.hpp"
#include "common/endian.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <span>
#include <string_view>
#include <vector>

namespace common {

class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::uint8_t> data, std::endian endian = std::endian::native) : mData(std::move(data)), mOffset(0ull), mEndian(endian) {}

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryRead(size_t offset, std::endian endian = std::endian::native) const -> std::optional<T> {
        if (offset + sizeof(T) > size()) {
            return std::nullopt;
        }

        T value;
        std::memcpy(&value, data() + offset, sizeof(T));
        InplaceByteSwapIfNeeded(value, endian);
        return std::make_optional(value);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryRead(std::endian endian = std::endian::native) -> std::optional<T> {
        const auto res = tryRead<T>(mOffset, endian);
        if (res) {
            mOffset += sizeof(T);
        }
        return res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(size_t offset, std::endian endian) const -> T {
        const auto res = tryRead<T>(offset, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T));
        }
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(std::endian endian) -> T {
        const auto res = tryRead<T>(mOffset, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T));
        }
        mOffset += sizeof(T);
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(size_t offset) const -> T {
        const auto res = tryRead<T>(offset, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T));
        }
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read() -> T {
        const auto res = tryRead<T>(mOffset, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T));
        }
        mOffset += sizeof(T);
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryReadArray(size_t offset, size_t numElements, std::endian endian = std::endian::native) const -> std::optional<std::vector<T>> {
        if (offset + sizeof(T) * numElements > size()) {
            return std::nullopt;
        }

        std::vector<T> value(numElements);
        if (endian == std::endian::native) {
            std::memcpy(value.data(), data() + offset, sizeof(T) * numElements);
        } else {
            const auto range = std::span{ reinterpret_cast<const T*>(data() + offset), numElements };
            std::transform(range.cbegin(), range.cend(), value.begin(), ByteSwap<T>);
        }
        return std::make_optional(value);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryReadArray(size_t numElements, std::endian endian = std::endian::native) -> std::optional<std::vector<T>> {
        const auto res = tryReadArray<T>(mOffset, numElements, endian);
        if (res) {
            mOffset += sizeof(T) * numElements;
        }
        return res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t offset, size_t numElements, std::endian endian) const -> std::vector<T> {
        const auto res = tryReadArray<T>(offset, numElements, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T) * numElements);
        }
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t numElements, std::endian endian) -> std::vector<T> {
        const auto res = tryReadArray<T>(mOffset, numElements, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T) * numElements);
        }
        mOffset += sizeof(T) * numElements;
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t offset, size_t numElements) const -> std::vector<T> {
        const auto res = tryReadArray<T>(offset, numElements, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T) * numElements);
        }
        return *res;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t numElements) -> std::vector<T> {
        const auto res = tryReadArray<T>(mOffset, numElements, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T) * numElements);
        }
        mOffset += sizeof(T) * numElements;
        return *res;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryReadArrayFixed(size_t offset, std::endian endian = std::endian::native) const -> std::optional<std::array<T, N>> {
        if (offset + sizeof(T) * N > size()) {
            return std::nullopt;
        }

        std::array<T, N> value;
        if (endian == std::endian::native) {
            std::memcpy(value.data(), data() + offset, sizeof(T) * N);
        } else {
            const auto range = std::span{ reinterpret_cast<const T*>(data() + offset), N };
            std::transform(range.cbegin(), range.cend(), value.begin(), ByteSwap<T>);
        }
        return std::make_optional(value);
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto tryReadArrayFixed(std::endian endian = std::endian::native) -> std::optional<std::array<T, N>> {
        const auto res = tryReadArrayFixed<T, N>(mOffset, endian);
        if (res) {
            mOffset += sizeof(T) * N;
        }
        return res;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(size_t offset, std::endian endian) const -> std::array<T, N> {
        const auto res = tryReadArrayFixed<T, N>(offset, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T) * N);
        }
        return *res;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(std::endian endian) -> std::array<T, N> {
        const auto res = tryReadArrayFixed<T, N>(mOffset, endian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T) * N);
        }
        mOffset += sizeof(T) * N;
        return *res;
    }

        template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(size_t offset) const -> std::array<T, N> {
        const auto res = tryReadArrayFixed<T, N>(offset, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(offset, sizeof(T) * N);
        }
        return *res;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed() -> std::array<T, N> {
        const auto res = tryReadArrayFixed<T, N>(mOffset, mEndian);
        if (!res) [[unlikely]] {
            onOutOfBoundsRead_(mOffset, sizeof(T) * N);
        }
        mOffset += sizeof(T) * N;
        return *res;
    }

    [[nodiscard]] auto tryReadString(size_t offset) -> std::optional<std::string_view> {
        if (offset >= size()) {
            return std::nullopt;
        }

        const auto str = reinterpret_cast<const char*>(data() + offset);
        const auto strSize = strnlen(str, size() - offset);
        return std::make_optional<std::string_view>(str, strSize);
    }

    [[nodiscard]] auto tryReadString() -> std::optional<std::string_view> {
        const auto res = tryReadString(mOffset);
        if (res) {
            mOffset += res->size();
        }
        return res;
    }

    [[nodiscard]] auto readString(size_t offset) -> std::string_view {
        const auto res = tryReadString(offset);
        if (!res) [[unlikely]] {
            onOutOfBoundsReadString_(offset);
        }
        return *res;
    }

    [[nodiscard]] auto readString() -> std::string_view {
        const auto res = tryReadString(mOffset);
        if (!res) [[unlikely]] {
            onOutOfBoundsReadString_(mOffset);
        }
        mOffset += res->size();
        return *res;
    }

    [[nodiscard]] auto data() const -> const std::uint8_t* {
        return mData.data();
    }

    [[nodiscard]] auto size() const -> size_t {
        return mData.size();
    }

    auto seek(size_t offset) -> void {
        if (offset <= size()) {
            mOffset = offset;
        }
    }

    auto skip(size_t offset) -> void {
        if (mOffset + offset <= size()) {
            mOffset += offset;
        } else {
            mOffset = size();
        }
    }

    auto rewind(size_t offset) -> void {
        if (static_cast<ssize_t>(mOffset - offset) >= 0) {
            mOffset -= offset;
        } else {
            mOffset = 0;
        }
    }

    [[nodiscard]] auto tell() const -> size_t {
        return mOffset;
    }

    [[nodiscard]] auto subspan(size_t offset, size_t spanSize) const -> BinaryReader {
        if (offset >= size() || spanSize == 0) {
            return BinaryReader({});
        }

        return BinaryReader({ data() + offset, std::min(size() - offset, spanSize) });
    }

    auto setEndian(std::endian endian) -> void {
        mEndian = endian;
    }

    [[nodiscard]] auto getEndian() const -> std::endian {
        return mEndian;
    }

    [[nodiscard]] auto checkSize(size_t size) const -> bool {
        return std::max(mData.size() - mOffset, 0ull) >= size;
    }

    auto alignUp(size_t align) -> void {
        if (align != 0) {
            const auto offset = common::AlignUp(mOffset, align);
            seek(offset);
        }
    }

private:
    [[noreturn]] auto onOutOfBoundsRead_(size_t offset, size_t readSize) const -> void;
    [[noreturn]] auto onOutOfBoundsReadString_(size_t offset) const -> void;

    std::span<const std::uint8_t> mData;
    size_t mOffset;
    std::endian mEndian;
};

} // namespace common
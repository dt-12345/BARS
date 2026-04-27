#include "common/reader.hpp"

#include <format>
#include <stacktrace>
#include <stdexcept>

namespace common {

struct OutOfBoundsReadException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

auto BinaryReader::onOutOfBoundsRead_(size_t offset, size_t readSize) const -> void {
    throw OutOfBoundsReadException(std::format(
        "[ERROR]: Tried to read past end of buffer!\n"
        "\tBuffer Size: {:#x}\n"
        "\tRead Offset: {:#x}\n"
        "\tRead Size:   {:#x}\n"
        "{}\n",
        size(), offset, readSize, std::stacktrace::current(1)
    ));
}

auto BinaryReader::onOutOfBoundsReadString_(size_t offset) const -> void {
    throw OutOfBoundsReadException(std::format(
        "[ERROR]: Tried to read string past end of buffer!\n"
        "\tBuffer Size: {:#x}\n"
        "\tRead Offset: {:#x}\n"
        "{}\n",
        size(), offset, std::stacktrace::current(1)
    ));
}

} // namespace common
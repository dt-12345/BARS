#include "common/hash.hpp"
#include "decode/amtaReader.hpp"
#include "decode/barsReader.hpp"
#include "decode/bwavReader.hpp"
#include "resource/bars.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace decode {

auto BarsReader::ReadAsset(common::BinaryReader& reader, sound::Asset& asset, const resource::ResAssetOffset& offset) -> Result {
    // note: both the amta and bwav readers will alter the reader endianness, but since we don't use it again, it's fine
    reader.seek(offset.metaOffset);
    if (const auto res = AmtaReader::Read(reader, asset.getMetadata()); res != OK) {
        return res;
    }

    reader.seek(offset.dataOffset);
    if (const auto res = BwavReader::Read(reader, asset.getSound(), &asset.getStreamInfo()); res != OK) {
        return res;
    }

    return OK;
}
    
auto BarsReader::Read(common::BinaryReader& reader, sound::Archive& archive) -> Result {
    if (!reader.checkSize(sizeof(resource::ResAudioResource))) {
        return BufferTooSmall;
    }

    const auto magic = reader.readArrayFixed<char, 4>();
    if (magic[0] != 'B' || magic[1] != 'A' || magic[2] != 'R' || magic[3] != 'S') {
        return InvalidBarsMagic;
    }

    reader.skip(4); // file size
    const auto bom = reader.read<std::uint16_t>(std::endian::big);
    reader.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);

    const auto version = reader.read<std::uint16_t>();
    if (version != 0x102 && version != 0x101) {
        return InvalidBarsVersion;
    }

    const auto assetCount = reader.read<std::uint32_t>();
    if (!reader.checkSize(assetCount * sizeof(std::uint32_t) + assetCount * sizeof(resource::ResAssetOffset))) {
        return BufferTooSmall;
    }

    const auto hashes = reader.readArray<std::uint32_t>(assetCount);
    const auto offsets = reader.readArray<resource::ResAssetOffset>(assetCount);

    if (version == 0x102 && !reader.checkSize(sizeof(std::uint32_t))) {
        return BufferTooSmall;
    }

    const auto publicAssetCount = version == 0x102 ? reader.read<std::uint32_t>() : 0u;
    if (version == 0x102 && !reader.checkSize(publicAssetCount * sizeof(std::uint32_t))) {
        return BufferTooSmall;
    }
    auto publicHashes = reader.readArray<std::uint32_t>(publicAssetCount);
    // these should already be sorted but just in case
    std::sort(publicHashes.begin(), publicHashes.end());

    archive.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);
    for (const auto& offset : offsets) {
        auto& asset = archive.addAsset();
        if (const auto res = ReadAsset(reader, asset, offset); res != OK) {
            return res;
        }

        asset.getMetadata().setIsPublic(std::binary_search(publicHashes.begin(), publicHashes.end(), common::CalcCRC32(asset.getName())));
    }

    return OK;
}

auto BarsReader::Read(common::BinaryReader& reader) -> std::expected<std::unique_ptr<sound::Archive>, Result> {
    auto archive = std::make_unique<sound::Archive>();
    if (const auto res = Read(reader, *archive); res != OK) {
        return std::expected<std::unique_ptr<sound::Archive>, Result>{ std::unexpected(res) };
    }
    return std::expected<std::unique_ptr<sound::Archive>, Result>{ std::in_place, std::move(archive) };
}

auto BarsReader::Read(std::span<const std::uint8_t> data) -> std::expected<std::unique_ptr<sound::Archive>, Result> {
    auto reader = common::BinaryReader(data);
    return Read(reader);
}

auto BarsReader::Read(std::string_view filepath) -> std::expected<std::unique_ptr<sound::Archive>, Result> {
    const auto path = std::filesystem::path(filepath);
    if (!std::filesystem::exists(path)) {
        return std::expected<std::unique_ptr<sound::Archive>, Result>{ std::unexpected(FileNotFound) };
    }

    const auto size = std::filesystem::file_size(path);
    auto infile = std::ifstream(path, std::ios::binary);
    if (!infile) {
        return std::expected<std::unique_ptr<sound::Archive>, Result>{ std::unexpected(FileNotFound) };
    }

    auto buffer = std::vector<std::uint8_t>(size);
    infile.read(reinterpret_cast<char*>(buffer.data()), size);

    return Read(buffer);
}

} // namespace decode
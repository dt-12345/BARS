#include "common/hash.hpp"
#include "encode/amtaWriter.hpp"
#include "encode/barsWriter.hpp"
#include "encode/bwavWriter.hpp"
#include "resource/bars.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <ranges>

namespace encode {

auto BarsWriter::Write(const sound::Archive& archive, common::BinaryWriter& writer) -> bool {
    writer.setEndian(archive.getEndian());
    const auto base = writer.tell();


    auto sortedAssets = std::map<std::uint32_t, std::reference_wrapper<const sound::Asset>>{};
    for (const auto& asset : archive.getAssets()) {
        sortedAssets.emplace(common::CalcCRC32(asset.getName()), std::cref(asset));
    }

    const auto publicAssets = sortedAssets
                            | std::views::filter([](const auto& element) { return element.second.get().getMetadata().getIsPublic(); })
                            | std::views::transform([](const auto& element) { return element.first; })
                            | std::ranges::to<std::vector>();

    writer.writeArrayFixed<char, 4>({ 'B', 'A', 'R', 'S' });
    writer.skip(4); // file size, fill in later
    writer.write(static_cast<std::uint16_t>(0xfeff));
    writer.write(publicAssets.size() ? static_cast<std::uint16_t>(0x102) : static_cast<std::uint16_t>(0x101));
    writer.write(static_cast<std::uint32_t>(sortedAssets.size()));
    for (const auto hash : sortedAssets | std::views::keys) {
        writer.write(hash);
    }
    
    writer.skip(sortedAssets.size() * sizeof(resource::ResAssetOffset));
    if (publicAssets.size()) {
        writer.write(static_cast<std::uint32_t>(publicAssets.size()));
        for (const auto hash : publicAssets) {
            writer.write(hash);
        }
    }

    const auto baseArrayOffset = base + sizeof(resource::ResAudioResource) + sortedAssets.size() * sizeof(std::uint32_t);
    for (const auto& [index, asset] : sortedAssets | std::views::values | std::views::enumerate) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        const auto atOffset = baseArrayOffset + index * sizeof(resource::ResAssetOffset) + offsetof(resource::ResAssetOffset, metaOffset);
        writer.writeAt(offset, atOffset);

        if (!AmtaWriter::Write(asset.get().getMetadata(), writer)) {
            return false;
        }
        writer.setEndian(archive.getEndian());
    }

    for (const auto& [index, asset] : sortedAssets | std::views::values | std::views::enumerate) {
        writer.alignUp(0x40);
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        const auto atOffset = baseArrayOffset + index * sizeof(resource::ResAssetOffset) + offsetof(resource::ResAssetOffset, dataOffset);
        writer.writeAt(offset, atOffset);

        if (asset.get().getMetadata().getIsStreaming()) {
            if (!BwavWriter::WritePrefetch(asset.get().getSound(), asset.get().getStreamInfo(), writer)) {
                return false;
            }
        } else {
            if (!BwavWriter::Write(asset.get().getSound(), writer)) {
                return false;
            }
        }
        writer.setEndian(archive.getEndian());
    }

    const auto size = static_cast<std::uint32_t>(writer.tell() - base);
    writer.writeAt(size, base + offsetof(resource::ResAudioResource, fileSize));
    return true;
}

auto BarsWriter::Write(const sound::Archive& archive, std::vector<std::uint8_t>& buffer) -> bool {
    auto writer = common::BinaryWriter();
    if (!Write(archive, writer)) {
        return false;
    }
    buffer.swap(writer.buffer());
    return true;
}

auto BarsWriter::Write(const sound::Archive& archive, std::string_view path) -> bool {
    auto buffer = std::vector<std::uint8_t>{};
    if (!Write(archive, buffer)) {
        return false;
    }

    const auto fpath = std::filesystem::path(path);
    if (!fpath.parent_path().empty()) {
        std::filesystem::create_directories(fpath.parent_path());
    }

    auto outfile = std::ofstream(fpath, std::ios::binary);
    if (!outfile) {
        return false;
    }

    outfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

} // namespace encode
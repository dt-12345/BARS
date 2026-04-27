#include "common/writer.hpp"
#include "encode/bwavWriter.hpp"
#include "sound/asset.hpp"

namespace sound {

auto Asset::setSound(std::string_view name, Sound& sound, bool keepMeta) -> bool {
    // do not call this function with prefetch sounds
    if (sound.isPrefetch()) {
        return false;
    }

    if (mEndian != sound.getEndian()) {
        sound.resampleAndConvert(mEndian, sound.getSampleFormat(), sound.getSampleRate());
    }

    mSound.reset();
    if (sound.requiresPrefetch()) {
        if (!sound.createPrefetch(mSound)) {
            return false;
        }
    } else {
        mSound = sound;
        sound.setPrefetchInfo(mSound);
    }

    if (!keepMeta) {
        mMetadata.reset();
    }
    // TODO: what are the conditions that causes a sound to be streamed with no prefetch?
    mMetadata.setIsStreaming(mSound.isPrefetch());
    mMetadata.setHasSoundInArchive(true);
    mMetadata.setName(name);
    mMetadata.calculate(sound);

    mStreamInfo.seekPoints.clear();
    mStreamInfo.sampleOffsets.clear();
    mStreamInfo.sampleCounts.clear();
    mStreamInfo.dataHash = 0u;
    if (mSound.isPrefetch()) {
        auto writer = common::BinaryWriter();
        if (!encode::BwavWriter::Write(sound, writer, &mStreamInfo)) {
            return false;
        }
    }

    return true;
}

} // namespace sound
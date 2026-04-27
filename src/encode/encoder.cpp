#include "encode/encoder.hpp"
#include "encode/pcmEncoder.hpp"
#include "encode/adpcmEncoder.hpp"
#include "encode/opusEncoder.hpp"
#include <memory>

namespace encode {

auto CreateEncoder(sound::Format fmt) -> std::unique_ptr<IEncoder> {
    switch (fmt) {
        case sound::Format::PcmInt16:
            return std::make_unique<PcmEncoder>();
        case sound::Format::DspAdpcm:
            return std::make_unique<AdpcmEncoder>();
        case sound::Format::Opus:
            return std::make_unique<OpusEncoder>();
        default:
            return std::make_unique<PcmEncoder>();
    }
}

} // namespace encode
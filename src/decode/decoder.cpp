#include "decode/decoder.hpp"
#include "decode/pcmDecoder.hpp"
#include "decode/adpcmDecoder.hpp"
#include "decode/opusDecoder.hpp"

namespace decode {

auto CreateDecoder(sound::Format fmt) -> std::unique_ptr<IDecoder> {
    switch (fmt) {
        case sound::Format::PcmInt16: return std::make_unique<Pcm16Decoder>();
        case sound::Format::DspAdpcm: return std::make_unique<AdpcmDecoder>();
        case sound::Format::Opus: return std::make_unique<OpusDecoder>();
        default: return std::make_unique<Pcm16Decoder>();
    }
}

auto CreateStreamDecoder(sound::Format fmt) -> std::unique_ptr<IStreamDecoder> {
    switch (fmt) {
        case sound::Format::PcmInt16: return std::make_unique<Pcm16StreamDecoder>();
        case sound::Format::DspAdpcm: return std::make_unique<AdpcmStreamDecoder>();
        case sound::Format::Opus: return std::make_unique<OpusStreamDecoder>();
        default: return std::make_unique<Pcm16StreamDecoder>();
    }
}

} // namespace decode
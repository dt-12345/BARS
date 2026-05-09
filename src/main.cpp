#include "decode/barsReader.hpp"
#include "decode/bwavReader.hpp"
#include "decode/wavReader.hpp"
#include "encode/bwavWriter.hpp"
#include "encode/barsWriter.hpp"
#include "playback/source.hpp"

#include <filesystem>
#include <iostream>
#include <print>
#include <string>

static const std::string ParseInput(int argc, const char** argv, int index) {
    if (argc < 2 + index) {
        return "";
    }

    size_t size = strnlen(argv[1 + index], 0x1000);
    std::string value{argv[1 + index], argv[1 + index] + size};
    return value;
}

static auto PlayerMain(const std::string_view inputPath) -> std::int32_t {
    auto sound = std::unique_ptr<sound::Sound>();
    if (inputPath.ends_with(".bwav")) {
        auto soundRes = decode::BwavReader::Read(inputPath);
        if (!soundRes) {
            std::println(std::cerr, "Failed to parse BWAV ({}): {}", inputPath, decode::ToString(soundRes.error()));
            return 1;
        }
        sound = std::move(*soundRes);
    } else {
        auto soundRes = decode::WavReader::Read(inputPath);
        if (!soundRes) {
            std::println(std::cerr, "Failed to parse WAV ({}): {}", inputPath, decode::ToString(soundRes.error()));
            return 1;
        }
        sound = std::move(*soundRes);
    }

    auto source = playback::SoundSource();
    if (!source.initialize(*sound)) {
        std::println(std::cerr, "Failed to initialize sound source");
        return 1;
    }

    if (!source.play()) {
        std::println(std::cerr, "Failed to start playback");
        return 1;
    }
    
    std::println("Currently playing {} ({} sec)", inputPath, sound->getDuration());
    std::println("Press S to stop");
    for (std::int32_t ch; (ch = std::getchar()) != EOF;) {
        if (std::isspace(static_cast<char>(ch))) {
            continue;
        }
        switch (static_cast<char>(ch)) {
            case 'r':
            case 'R':
                source.seek(0);
                std::println("Skipped to beginning");
                break;
            case 'f':
            case 'F': {
                const auto cursor = source.seek(sound->getSampleRate() * 5.f, true);
                std::println("Skipped to {} seconds", static_cast<float>(cursor) / static_cast<float>(sound->getSampleRate()));
                break;
            }
            case 'b':
            case 'B': {
                const auto cursor = source.seek(sound->getSampleRate() * -5.f, true);
                std::println("Skipped to {} seconds", static_cast<float>(cursor) / static_cast<float>(sound->getSampleRate()));
                break;
            }
            case 's':
            case 'S':
                source.stop();
                return 0;
            case 'p':
            case 'P':
                source.pause();
                std::println("Paused");
                break;
            case 'u':
            case 'U':
                source.play();
                std::println("Unpaused");
                break;
            case 't':
            case 'T':
                std::println("Current Time: {}", static_cast<float>(source.tell()) / static_cast<float>(source.getSampleRate()));
                break;
            default:
                std::println("Press S to stop");
                break;
        }
    }

    return 0;
}

static auto PatcherMain(
    const std::string_view barsPath,
    const std::string_view soundPath,
    const std::string_view soundName,
    const std::string_view outputPath,
    const std::string_view outputBwavPath,
    std::optional<std::uint32_t> sampleRate,
    std::optional<sound::Format> format,
    std::optional<std::endian> endian,
    bool keepMeta
) -> std::int32_t {
    auto sound = std::unique_ptr<sound::Sound>();
    if (soundPath.ends_with(".bwav")) {
        auto soundRes = decode::BwavReader::Read(soundPath);
        if (!soundRes) {
            std::println(std::cerr, "Failed to parse BWAV ({}): {}", soundPath, decode::ToString(soundRes.error()));
            return 1;
        }
        sound = std::move(*soundRes);
    } else {
        auto soundRes = decode::WavReader::Read(soundPath);
        if (!soundRes) {
            std::println(std::cerr, "Failed to parse WAV ({}): {}", soundPath, decode::ToString(soundRes.error()));
            return 1;
        }
        sound = std::move(*soundRes);
    }

    if (sampleRate || format || endian) {
        std::println("Processing {}...", soundPath);
        if (!sound->resampleAndConvert(
            endian ? *endian : sound->getEndian(),
            format ? *format : sound->getSampleFormat(),
            sampleRate ? *sampleRate : sound->getSampleRate()
        )) {
            std::println(std::cerr, "Failed to resample and convert {}", soundPath);
            return 1;
        }
    }

    if (!outputBwavPath.empty()) {
        if (!encode::BwavWriter::Write(*sound, outputBwavPath)) {
            std::println(std::cerr, "Failed to convert WAV to BWAV!");
            return 1;
        }
    }

    auto archive = std::unique_ptr<sound::Archive>();
    if (barsPath == "-") {
        archive = std::make_unique<sound::Archive>();
        archive->setEndian(std::endian::native);
    } else {
        auto archiveRes = decode::BarsReader::Read(barsPath);
        if (!archiveRes) {
            std::println(std::cerr, "Failed to parse BARS ({}): {}", barsPath, decode::ToString(archiveRes.error()));
            return 1;
        }
        archive = std::move(*archiveRes);
    }

    const auto name = soundName.empty() || soundName == "-" ? std::filesystem::path(soundPath).stem() : soundName;
    if (const auto existing = archive->getAsset(soundName); existing != nullptr) {
        if (!existing->setSound(name.string(), *sound, keepMeta)) {
            std::println(std::cerr, "Failed to replace {}", name.string());
            return 1;
        }
    } else {
        auto& asset = archive->addAsset();
        if (!asset.setSound(name.string(), *sound, false)) {
            std::println(std::cerr, "Failed to add {}", name.string());
            return 1;
        }
    }

    return encode::BarsWriter::Write(*archive, outputPath.empty() ? barsPath : outputPath) ? 0 : 1;
}

static auto ConverterMain(
    const std::string_view inputPath,
    const std::string_view outputPath,
    std::optional<std::uint32_t> sampleRate,
    std::optional<sound::Format> format,
    std::optional<std::endian> endian
) -> std::int32_t {
    auto soundRes = decode::WavReader::Read(inputPath);
    if (!soundRes) {
        std::println(std::cerr, "Failed to parse WAV ({}): {}", inputPath, static_cast<std::uint32_t>(soundRes.error()));
        return 1;
    }

    auto sound = std::move(*soundRes);
    if (sampleRate || format || endian) {
        std::println("Processing {}...", inputPath);
        if (!sound->resampleAndConvert(
            endian ? *endian : sound->getEndian(),
            format ? *format : sound->getSampleFormat(),
            sampleRate ? *sampleRate : sound->getSampleRate()
        )) {
            std::println(std::cerr, "Failed to resample and convert {}", inputPath);
            return 1;
        }
    }

    return encode::BwavWriter::Write(*sound, outputPath) ? 0 : 1;
}

static auto DumperMain(const std::string_view inputPath, const std::string_view assetName) -> std::int32_t {
    auto archive = decode::BarsReader::Read(inputPath);
    if (!archive) {
        std::println(std::cerr, "Failed to parse BARS ({}): {}", inputPath, decode::ToString(archive.error()));
        return 1;
    }

    if (assetName.empty()) {
        for (const auto& asset : archive.value()->getAssets()) {
            std::print("{}", asset.dumpMetadata());
        }
    } else {
        if (const auto asset = archive.value()->getAsset(assetName); asset != nullptr) {
            std::print("{}", asset->dumpMetadata());
        } else {
            std::println(std::cerr, "{} does not exit", assetName);
            return 1;
        }
    }
    return 0;
}

// TODO: UI
auto main(int argc, const char** argv) -> int {
    auto optIndex = 0;
    auto opt = ParseInput(argc, argv, optIndex++);
    std::transform(opt.begin(), opt.end(), opt.begin(), [](unsigned char c){ return std::tolower(c); });

    if (opt == "play") {
        const auto inputPath = ParseInput(argc, argv, optIndex++);
        return PlayerMain(inputPath);
    } else if (opt == "patch") {
        const auto barsPath = ParseInput(argc, argv, optIndex++);
        const auto soundPath = ParseInput(argc, argv, optIndex++);
        auto soundName = std::string("");
        auto outputBarsPath = std::format("{}.bars", std::filesystem::path(barsPath).stem().string());
        auto outputBwavPath = std::string("");
        auto sampleRate = std::optional<std::uint32_t>();
        auto endian = std::optional<std::endian>();
        auto sampleFormat = std::optional<sound::Format>();
        auto keepMeta = true;
        while (optIndex < argc) {
            const auto arg = ParseInput(argc, argv, optIndex++);
            if (arg == "--out" || arg == "-o") {
                outputBarsPath = ParseInput(argc, argv, optIndex++);
            } else if (arg == "--out-bwav" || arg == "-ob") {
                outputBwavPath = ParseInput(argc, argv, optIndex++);
            } else if (arg == "--name" || arg == "-n") {
                soundName = ParseInput(argc, argv, optIndex++);
            } else if (arg == "--sample-rate" || arg == "-fs" || arg == "-sr") {
                const auto fs = ParseInput(argc, argv, optIndex++);
                sampleRate = std::atoi(fs.c_str());
                if (sampleRate == 0u) {
                    std::println(std::cerr, "[WARNING] Invalid sample rate: {}", fs);
                    sampleRate.reset();
                }
            } else if (arg == "--endian" || arg == "-e") {
                const auto byteOrder = ParseInput(argc, argv, optIndex++);
                if (byteOrder == "big") {
                    endian = std::endian::big;
                } else if (byteOrder == "little") {
                    endian = std::endian::little;
                } else {
                    std::println(std::cerr, "[WARNING] Unknown endianness: {}", byteOrder);
                }
            } else if (arg == "--format" || arg == "-f") {
                const auto fmt = ParseInput(argc, argv, optIndex++);
                if (fmt == "pcm" || fmt == "pcm16" || fmt == "pcmint16") {
                    sampleFormat = sound::Format::PcmInt16;
                } else if (fmt == "adpcm" || fmt == "dspadpcm" || fmt == "dsp-adpcm") {
                    sampleFormat = sound::Format::DspAdpcm;
                } else if (fmt == "opus") {
                    sampleFormat = sound::Format::Opus;
                } else {
                    std::println(std::cerr, "[WARNING] Unknown sample format: {}", fmt);
                }
            } else if (arg == "--overwrite-meta" || arg == "-om") {
                keepMeta = false;
            }
        }
        return PatcherMain(
            barsPath, soundPath, soundName, outputBarsPath, outputBwavPath, 
            std::move(sampleRate), std::move(sampleFormat), std::move(endian), keepMeta
        );
    } else if (opt == "convert") {
        const auto inputPath = ParseInput(argc, argv, optIndex++);
        auto outputPath = std::format("{}.bwav", std::filesystem::path(inputPath).stem().string());
        auto sampleRate = std::optional<std::uint32_t>();
        auto endian = std::optional<std::endian>();
        auto sampleFormat = std::optional<sound::Format>();
        while (optIndex < argc) {
            const auto arg = ParseInput(argc, argv, optIndex++);
            if (arg == "--out" || arg == "-o") {
                outputPath = ParseInput(argc, argv, optIndex++);
            } else if (arg == "--sample-rate" || arg == "-fs" || arg == "-sr") {
                const auto fs = ParseInput(argc, argv, optIndex++);
                sampleRate = std::atoi(fs.c_str());
                if (sampleRate == 0u) {
                    std::println(std::cerr, "[WARNING] Invalid sample rate: {}", fs);
                    sampleRate.reset();
                }
            } else if (arg == "--endian" || arg == "-e") {
                const auto byteOrder = ParseInput(argc, argv, optIndex++);
                if (byteOrder == "big") {
                    endian = std::endian::big;
                } else if (byteOrder == "little") {
                    endian = std::endian::little;
                } else {
                    std::println(std::cerr, "[WARNING] Unknown endianness: {}", byteOrder);
                }
            } else if (arg == "--format" || arg == "-f") {
                const auto fmt = ParseInput(argc, argv, optIndex++);
                if (fmt == "pcm" || fmt == "pcm16" || fmt == "pcmint16") {
                    sampleFormat = sound::Format::PcmInt16;
                } else if (fmt == "adpcm" || fmt == "dspadpcm" || fmt == "dsp-adpcm") {
                    sampleFormat = sound::Format::DspAdpcm;
                } else if (fmt == "opus") {
                    sampleFormat = sound::Format::Opus;
                } else {
                    std::println(std::cerr, "[WARNING] Unknown sample format: {}", fmt);
                }
            }
        }
        return ConverterMain(inputPath, outputPath, std::move(sampleRate), std::move(sampleFormat), std::move(endian));
    } else if (opt == "dump") {
        const auto inputPath = ParseInput(argc, argv, optIndex++);
        const auto assetName = ParseInput(argc, argv, optIndex++);
        return DumperMain(inputPath, assetName);
    } else if (opt == "help") {
        auto cmd = ParseInput(argc, argv, optIndex++);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return std::tolower(c); });
        if (cmd == "play") {
            std::print(
                "Music Player:\n"
                "  Usage:\n"
                "    bars play <path_to_bwav_file>\n"
                "    bars play <path_to_wav_file>\n"
                "  Controls:\n"
                "    r - restart player\n"
                "    s - stop player\n"
                "    p - pause player\n"
                "    u - unpause player\n"
                "    f - move forward 5 seconds\n"
                "    b - move backward 5 seconds\n"
                "    b - print current time\n"
            );
        } else if (cmd == "patch") {
            std::print(
                "BARS Patcher:\n"
                "  Usage:\n"
                "    bars patch <path_to_input_bars> <path_to_audio_file>\n"
                "  Options:\n"
                "    --name <sound_name>\n"
                "    --out <path_to_output_bars>\n"
                "    --out-bwav <path_to_output_bwav>\n"
                "    --sample-rate <sample_rate>\n"
                "    --endian <endianness>\n"
                "      Choices: big, little\n"
                "    --format <sample_format>\n"
                "      Choices: pcm, adpcm, opus\n"
                "    --overwrite-meta\n"
            );
        } else if (cmd == "convert") {
            std::print(
                "BWAV Converter:\n"
                "  Usage:\n"
                "    bars convert <path_to_input_wav>\n"
                "  Options:\n"
                "    --out <path_to_output_bwav>\n"
                "    --sample-rate <sample_rate>\n"
                "    --endian <endianness>\n"
                "      Choices: big, little\n"
                "    --format <sample_format>\n"
                "      Choices: pcm, adpcm, opus\n"
            );
        } else if (cmd == "dump") {
            std::print(
                "Metadata Dumper:\n"
                "  Usage:\n"
                "    bars dump <path_to_bars>\n"
                "    bars dump <path_to_bars> <asset_name>\n"
            );
        } else {
            std::println("Unknown command: {}\nPlease see usage for commands", cmd);
        }
#ifdef BARS_DEBUG
    } else if (opt == "parse") {
        const auto inputPath = ParseInput(argc, argv, optIndex++);
        if (inputPath.ends_with(".bars")) {
            const auto res = decode::BarsReader::Read(inputPath);
            if (!res) {
                std::println(std::cerr, "{}", decode::ToString(res.error()));
                return 1;
            }
        } else if (inputPath.ends_with(".bwav")) {
            const auto res = decode::BwavReader::Read(inputPath);
            if (!res) {
                std::println(std::cerr, "{}", decode::ToString(res.error()));
                return 1;
            }
        } else if (inputPath.ends_with(".wav")) {
            const auto res = decode::WavReader::Read(inputPath);
            if (!res) {
                std::println(std::cerr, "{}", decode::ToString(res.error()));
                return 1;
            }
        }
    } else if (opt == "rt") {
        const auto inputPath = ParseInput(argc, argv, optIndex++);
        const auto outputPath = ParseInput(argc, argv, optIndex++);
        if (inputPath.ends_with(".bars")) {
            const auto res = decode::BarsReader::Read(inputPath);
            if (!res) {
                std::println(std::cerr, "{}", decode::ToString(res.error()));
                return 1;
            }
            if (!encode::BarsWriter::Write(*res.value(), outputPath)) {
                std::println(std::cerr, "Failed to serialize file");
                return 1;
            }
        } else if (inputPath.ends_with(".bwav")) {
            const auto res = decode::BwavReader::Read(inputPath);
            if (!res) {
                std::println(std::cerr, "{}", decode::ToString(res.error()));
                return 1;
            }
            if (!encode::BwavWriter::Write(*res.value(), outputPath)) {
                std::println(std::cerr, "Failed to serialize file");
                return 1;
            }
        }
#endif
    } else {
        std::print(
            "Usage:\n"
            "  bars <cmd> <arguments>\n"
            "\n"
            "Commands:\n"
            " - play (basic audio player)\n"
            " - patch (BARS file patcher - add or swap assets)\n"
            " - convert (BWAV converter - convert WAV files into BWAV with selected parameters)\n"
            " - help (command usage help)\n"
        );
    }

    return 0;
}
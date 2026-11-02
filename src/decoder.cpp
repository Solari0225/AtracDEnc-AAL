#include "decoder_api.hpp"
#include "aal_format.hpp"
#include "platform_io.hpp"
#include "sha256.hpp"
#include "../compression/enhancement.hpp"
#include "../core/decode/aal3/fixed_predictor.hpp"
#include "../core/decode/aalp/core_reader.h"
#include <memory>

namespace independent_aal {
namespace {
constexpr size_t limit = size_t(1) << 30;
void notify(const Progress& progress, const std::atomic_bool* cancelled,
            int percent, const wchar_t* stage) {
    if (cancelled && cancelled->load()) throw std::runtime_error("Cancelled");
    if (progress) progress(percent, stage);
}
}

DecodeBuffer decode_aal(const uint8_t* data, size_t bytes, const Progress& progress,
                        const std::atomic_bool* cancelled) {
    if (!data || bytes < 96 || bytes > limit)
        throw std::runtime_error("Invalid or oversized AAL input (maximum 1 GiB).");
    notify(progress, cancelled, 0, L"Read AAL container");
    const auto parsed = parse_aal(Bytes(data, data + bytes));
    const auto& profile = *parsed.profile;
    const size_t samples = profile.frame_samples;
    const size_t count = parsed.packets.size();
    const size_t tail = parsed.packets.back().tail;
    // Sony's final count is the unshifted source remainder. A source tail
    // inside the synthesis delay needs an additional flush packet. Duration
    // milliseconds do not encode the exact source length.
    const uint64_t declared = uint64_t(count - 2) * samples + tail;
    const size_t carry = tail + profile.pcm_trim > samples ? samples : 0;
    if (declared <= carry || declared - carry > limit / 4)
        throw std::runtime_error("Invalid or oversized decoded PCM (maximum 1 GiB).");
    const size_t frames = size_t(declared - carry);
    if (profile_block_count(uint32_t(frames), profile) != count ||
        be16(parsed.header.data() + 34) != tail)
        throw std::runtime_error("Inconsistent AAL sample counts.");
    DecodeBuffer result;
    result.profile_id = profile.id;
    result.pcm.resize(frames * 2);
    independent_aal_decoder::OriginalFixedPredictor atrac3;
    auto plus = std::make_unique<sony_aalp_predictor_state>();
    sony_original_aalp_predictor_reset(plus.get());
    const uint32_t cap = profile.core_bytes <= 744 ? 8 : profile.core_bytes <= 1488 ? 12 : 16;
    size_t written = 0;
    for (size_t ordinal = 0; ordinal < count; ++ordinal) {
        if (ordinal % 16 == 0)
            notify(progress, cancelled, 1 + int(ordinal * 94 / count), L"Decode AAL core and lossless enhancement");
        const auto& packet = parsed.packets[ordinal];
        if ((!packet.last && packet.tail != samples) ||
            (ordinal && (packet.enhancement.empty() || packet.enhancement.size() > samples * 4 + 8 ||
                         packet.enhancement.size() % 8 || packet.padding >= packet.enhancement.size())) ||
            (!ordinal && packet.padding))
            throw std::runtime_error("Invalid AAL enhancement framing.");
        std::array<int16_t, 4096> predicted{};
        if (profile.codec == CoreCodec::Atrac3) {
            const auto pcm = atrac3.decode(packet.core.data());
            std::copy(pcm.begin(), pcm.end(), predicted.begin());
        } else {
            sony_aalp_core_fields fields{};
            uint32_t positions[10];
            const int stage = sony_original_aalp_read_core(packet.core.data(), uint32_t(packet.core.size()), &fields, positions);
            if (stage || fields.channels != 2)
                throw std::runtime_error("ATRAC3plus core parse failed at block " + std::to_string(ordinal) +
                                         ", stage " + std::to_string(stage) + ".");
            int16_t planar[2][2048];
            if (sony_original_aalp_predict(&fields, cap, plus.get(), planar))
                throw std::runtime_error("Invalid ATRAC3plus integer prediction.");
            for (size_t j = 0; j < samples; ++j)
                for (size_t ch = 0; ch < 2; ++ch) predicted[2 * j + ch] = planar[ch][j];
        }
        if (!ordinal) continue;
        compression::EnhancementPacket enhancement;
        enhancement.bytes = packet.enhancement;
        enhancement.padding_bytes = packet.padding;
        const auto restore = [&](const auto& residual) {
            for (size_t j = 0; j < samples; ++j) {
                const int64_t frame = (int64_t(ordinal) - 1) * int64_t(samples) + int64_t(j) - profile.pcm_trim;
                if (frame < 0 || uint64_t(frame) >= frames) continue;
                for (size_t ch = 0; ch < 2; ++ch) {
                    // The original enhancement adds modulo 16 bits; clipping
                    // this sum destroys losslessness around full scale.
                    const uint16_t word = uint16_t(uint16_t(predicted[j * 2 + ch]) + uint16_t(residual[ch][j]));
                    std::memcpy(&result.pcm[size_t(frame) * 2 + ch], &word, 2);
                }
                ++written;
            }
        };
        if (profile.codec == CoreCodec::Atrac3plus) restore(compression::decode_plus_enhancement(enhancement));
        else restore(compression::decode_enhancement(enhancement));
    }
    if (written != frames) throw std::runtime_error("Incomplete AAL PCM reconstruction.");
    notify(progress, cancelled, 97, L"Hash decoded PCM");
    result.pcm_sha256 = sha256(result.pcm.data(), result.pcm.size() * sizeof(int16_t));
    return result;
}

DecodeSummary decode_file(const std::filesystem::path& input, const std::filesystem::path& output,
                          const Progress& progress, const std::atomic_bool* cancelled) {
    const auto destination = std::filesystem::absolute(output).lexically_normal();
    if (std::filesystem::exists(destination)) throw std::runtime_error("Output already exists; choose another name.");
    if (!std::filesystem::is_directory(destination.parent_path())) throw std::runtime_error("Output directory does not exist.");
    std::ifstream file(input, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open input file.");
    const auto size = file.tellg();
    if (size < 96 || uint64_t(size) > limit) throw std::runtime_error("Invalid or oversized AAL input (maximum 1 GiB).");
    Bytes input_bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(input_bytes.data()), std::streamsize(input_bytes.size())))
        throw std::runtime_error("Incomplete input read.");
    auto decoded = decode_aal(input_bytes.data(), input_bytes.size(), progress, cancelled);
    const auto wave = write_wave(Wave{std::move(decoded.pcm)});
    notify(progress, cancelled, 99, L"Save decoded PCM WAV");
    auto temporary = destination;
    temporary += temporary_token();
    try {
        write_new(temporary, wave.data(), wave.size());
        if (cancelled && cancelled->load()) throw std::runtime_error("Cancelled");
        publish_new(temporary, destination);
    } catch (...) { remove_owned_file(temporary); throw; }
    DecodeSummary summary;
    summary.output_path = destination;
    summary.sample_frames = uint32_t((wave.size() - 44) / 4);
    summary.output_bytes = wave.size();
    summary.pcm_sha256 = std::move(decoded.pcm_sha256);
    summary.profile_id = std::move(decoded.profile_id);
    return summary;
}
}

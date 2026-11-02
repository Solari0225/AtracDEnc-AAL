#include "../include/independent_aal.h"
#include "decoder_api.hpp"
#include "platform_io.hpp"
#include <cstdlib>
#include <cstring>

extern "C" aal_status aal_decode_cd16(const uint8_t* oma, size_t bytes, aal_pcm_buffer* output,
                                      aal_progress callback, void* context, char* error, size_t capacity) {
    if (error && capacity) error[0] = 0;
    bool cancelled = false;
    const auto fail = [&](const char* message) {
        if (error && capacity) {
            const size_t n = std::min(capacity - 1, std::strlen(message));
            std::memcpy(error, message, n); error[n] = 0;
        }
    };
    try {
        if (!output || output->data) throw std::runtime_error("Output must be a zero-initialized aal_pcm_buffer.");
        auto progress = [&](int percent, const std::wstring& stage) {
            if (callback) {
                const auto text = independent_aal::utf8(stage);
                if (callback(percent, text.c_str(), context)) { cancelled = true; throw std::runtime_error("Cancelled"); }
            }
        };
        auto decoded = independent_aal::decode_aal(oma, bytes, progress);
        auto* pcm = static_cast<int16_t*>(std::malloc(decoded.pcm.size() * sizeof(int16_t)));
        if (!pcm) throw std::bad_alloc();
        std::memcpy(pcm, decoded.pcm.data(), decoded.pcm.size() * sizeof(int16_t));
        aal_pcm_buffer result{};
        result.data = pcm; result.sample_frames = decoded.pcm.size() / 2;
        result.sample_rate = 44100; result.channels = 2; result.bits_per_sample = 16;
        std::memcpy(result.profile_id, decoded.profile_id.c_str(), decoded.profile_id.size() + 1);
        std::memcpy(result.pcm_sha256, decoded.pcm_sha256.c_str(), 65);
        *output = result;
        return AAL_OK;
    } catch (const std::exception& e) { fail(e.what()); return cancelled ? AAL_CANCELLED : AAL_INVALID_OR_FAILED; }
    catch (...) { fail("Unknown decoder failure."); return AAL_INVALID_OR_FAILED; }
}
extern "C" void aal_pcm_free(aal_pcm_buffer* buffer) {
    if (!buffer) return;
    std::free(buffer->data);
    *buffer = aal_pcm_buffer{};
}

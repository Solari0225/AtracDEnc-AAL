// Offline official-file oracle and public decoder ABI checks. No Sony runtime.
#include "../include/independent_aal.h"
#include "../src/aal_format.hpp"
#include "../src/decoder_api.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

using independent_aal::Bytes;
static void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static int cancel(int, const char*, void*) { return 1; }
static int cancel_late(int percent, const char*, void*) { return percent >= 97; }
extern "C" int check_decode_c_abi(void);
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Supply decoder-vectors.bin");
        std::ifstream file(argv[1], std::ios::binary);
        Bytes fixture{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        require(fixture.size() >= 12 && !std::memcmp(fixture.data(), "AALDV001", 8), "Invalid decoder fixture");
        size_t at = 8;
        const auto number = [&]() { require(at + 4 <= fixture.size(), "Truncated fixture header");
            uint32_t n = independent_aal::le32(fixture.data() + at); at += 4; return n; };
        const uint32_t count = number();
        require(count && count < 1000, "Invalid fixture count");
        uint64_t compared = 0;
        Bytes small;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t name_size = number(), oma_size = number(), pcm_size = number();
            require(uint64_t(name_size) + oma_size + pcm_size <= fixture.size() - at, "Truncated decoder fixture");
            std::string name(reinterpret_cast<char*>(fixture.data() + at), name_size); at += name_size;
            const auto* oma = fixture.data() + at; at += oma_size;
            const auto* expected = fixture.data() + at; at += pcm_size;
            aal_pcm_buffer output{}; char error[512];
            require(aal_decode_cd16(oma, oma_size, &output, nullptr, nullptr, error, sizeof(error)) == AAL_OK, error);
            require(output.sample_rate == 44100 && output.channels == 2 && output.bits_per_sample == 16,
                    "Incorrect PCM description");
            require(output.data && output.sample_frames * 4 == pcm_size, "Incorrect decoded sample count");
            require(std::memcmp(output.data, expected, pcm_size) == 0, "Official-file PCM mismatch");
            require(std::strlen(output.pcm_sha256) == 64 && std::strlen(output.profile_id) > 0,
                    "Missing decoder result metadata");
            // Reject attempts to replace an owned buffer and preserve ownership.
            int16_t* owned = output.data;
            require(aal_decode_cd16(oma, oma_size, &output, nullptr, nullptr, error, sizeof(error)) != AAL_OK &&
                    output.data == owned, "Decoder replaced an owned result");
            compared += pcm_size / 2;
            aal_pcm_free(&output);
            require(!output.data && !output.sample_frames && !output.sample_rate, "PCM free did not clear output");
            aal_pcm_free(&output);
            if (small.empty() && oma_size < 20000 && pcm_size < 100) small.assign(oma, oma + oma_size);
        }
        require(at == fixture.size() && !small.empty(), "Unexpected fixture trailer or missing small case");
        char error[100]; aal_pcm_buffer output{};
        for (auto callback : {cancel, cancel_late}) {
            require(aal_decode_cd16(small.data(), small.size(), &output, callback, nullptr, error, sizeof(error)) == AAL_CANCELLED,
                    "Cancellation did not propagate");
            require(!output.data && !output.sample_frames, "Cancelled decode returned PCM");
        }
        size_t rejected = 0;
        const auto reject = [&](const Bytes& data) {
            output = {}; std::memset(error, '!', sizeof(error));
            require(aal_decode_cd16(data.data(), data.size(), &output, nullptr, nullptr, error, sizeof(error)) == AAL_INVALID_OR_FAILED,
                    "Malformed AAL was accepted");
            require(!output.data && !output.sample_frames && std::memchr(error, 0, sizeof(error)), "Invalid failure result");
            ++rejected;
        };
        for (size_t length : {size_t(0),size_t(1),size_t(95),size_t(96),small.size()-1})
            reject(Bytes(small.begin(), small.begin() + length));
        auto bad = small; bad[0] ^= 1; reject(bad);
        bad = small; bad[6] = 0; reject(bad); // Encryption indicator.
        bad = small; bad[32] = 1; reject(bad); // Ordinary lossy ATRAC is not AAL.
        bad = small; bad[43] ^= 1; reject(bad); // Declared packet count.
        bad = small; bad[104] = 1; reject(bad); // Packet ordinal.
        bad = small; bad[100] = bad[101] = 0; reject(bad); // Core payload size.
        require(check_decode_c_abi() == 0, "Decoder C ABI failure");
        // Structured, bounded mutations exercise physical core and residual
        // parsers. Accepted variants may change PCM; rejected ones own no data.
        uint32_t random = 0x27a14d39;
        for (unsigned i = 0; i < 512; ++i) {
            bad = small;
            random = random * 1664525u + 1013904223u;
            const size_t offset = 120 + random % (bad.size() - 120);
            bad[offset] ^= uint8_t(1u << ((random >> 24) & 7));
            output = {};
            const auto status = aal_decode_cd16(bad.data(), bad.size(), &output, nullptr, nullptr, error, sizeof(error));
            if (status == AAL_OK) {
                require(output.sample_frames == 1 && output.data, "Mutation changed declared source length");
                aal_pcm_free(&output);
            } else require(status == AAL_INVALID_OR_FAILED && !output.data, "Mutation failure owns data");
        }
        std::cout << "PASS: " << count << " official/syntax AAL files, " << compared
                  << " signed16 samples exact; ownership, cancellation, " << rejected
                  << " malformed inputs, 512 bounded core/residual mutations and C ABI\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}

#pragma once
// Independently written AAL bit packing and bounded Golomb-Rice mathematics.
// No external codec, process, DLL or reference instruction bytes are used.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace independent_aal { namespace compression {

class BitWriter {
    std::vector<uint8_t> bytes_;
    size_t bits_ = 0;
public:
    void put(uint32_t value, unsigned width) {
        if (width > 32) throw std::invalid_argument("Bit field exceeds 32 bits");
        for (unsigned n = width; n != 0; --n) {
            if ((bits_ & 7) == 0) bytes_.push_back(0);
            if ((value >> (n - 1)) & 1u) bytes_.back() |= uint8_t(1u << (7 - (bits_ & 7)));
            ++bits_;
        }
    }
    void align_byte() { const unsigned n = unsigned((-bits_) & 7); if (n) put(0, n); }
    size_t bit_count() const { return bits_; }
    const std::vector<uint8_t>& data() const { return bytes_; }
};

class BitReader {
    const uint8_t* bytes_;
    size_t size_, bits_ = 0;
public:
    BitReader(const uint8_t* data, size_t size) : bytes_(data), size_(size) {}
    explicit BitReader(const std::vector<uint8_t>& bytes) : BitReader(bytes.data(), bytes.size()) {}
    uint32_t get(unsigned width) {
        if (width > 32 || bits_ > size_ * 8 || width > size_ * 8 - bits_)
            throw std::runtime_error("Truncated AAL bit field");
        uint32_t value = 0;
        for (unsigned n = 0; n < width; ++n, ++bits_)
            value = (value << 1) | ((bytes_[bits_ >> 3] >> (7 - (bits_ & 7))) & 1u);
        return value;
    }
    void align_byte() { const unsigned n = unsigned((-bits_) & 7); if (get(n)) throw std::runtime_error("Nonzero AAL alignment bits"); }
    size_t bit_count() const { return bits_; }
};

inline uint32_t fold_signed(int32_t value) {
    return value < 0 ? uint32_t(-int64_t(value) * 2 - 1) : uint32_t(value) * 2u;
}
inline int32_t unfold_signed(uint32_t value) {
    return (value & 1u) ? -int32_t(value >> 1) - 1 : int32_t(value >> 1);
}
inline void validate_width(unsigned width) {
    if (!width || width > 31) throw std::invalid_argument("Rice width must be 1..31");
}

// The official selector deliberately scores an escape as width+16. The actual
// writer additionally emits its k remainder bits. Keeping these separate is
// needed to reproduce the original parameter choice, including rare escapes.
inline uint64_t rice_selection_cost(const int32_t* input, size_t count, unsigned width, unsigned k) {
    uint64_t cost = 0;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t q = fold_signed(input[i]) >> k;
        cost += q < 15 ? q + k + 1 : width + 16;
    }
    return cost;
}
inline unsigned choose_rice_k(const int32_t* input, size_t count, unsigned width = 16) {
    validate_width(width);
    uint64_t best = std::numeric_limits<uint64_t>::max();
    unsigned selected = 0;
    for (unsigned k = 0; k < width; ++k) {
        const uint64_t cost = rice_selection_cost(input, count, width, k);
        if (cost < best) { best = cost; selected = k; }
    }
    return selected;
}
inline uint64_t rice_written_bits(const int32_t* input, size_t count, unsigned width, unsigned k) {
    uint64_t bits = 5 + uint64_t(count) * k;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t q = fold_signed(input[i]) >> k;
        bits += q < 15 ? q + 1 : width + 16;
    }
    return bits;
}

inline void write_quotient(BitWriter& writer, uint32_t q, unsigned width) {
    if (q >= 15) { writer.put(0x5554, 16); writer.put(q, width); return; }
    for (unsigned j = 0; j < q; ++j) writer.put(j & 1u, 1);
    writer.put((q & 1u) ? 0u : 1u, 1);
}
inline uint32_t read_quotient(BitReader& reader, unsigned width) {
    for (unsigned q = 0; q < 16; ++q) {
        const uint32_t bit = reader.get(1);
        if (q == 15) {
            if (bit != 0) throw std::runtime_error("Invalid AAL Rice escape");
            return reader.get(width);
        }
        if (bit != (q & 1u)) return q;
    }
    throw std::runtime_error("Invalid AAL Rice quotient");
}

// Includes the five-bit k field. Quotients precede all remainders in a block.
inline unsigned write_rice_block(BitWriter& writer, const int32_t* input, size_t count,
                                 unsigned width = 16, int requested_k = -1) {
    validate_width(width);
    const unsigned k = requested_k < 0 ? choose_rice_k(input, count, width) : unsigned(requested_k);
    if (k >= width) throw std::invalid_argument("Rice k exceeds sample width");
    writer.put(k, 5);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t u = fold_signed(input[i]);
        if ((u >> width) != 0) throw std::range_error("Residual exceeds AAL sample width");
        write_quotient(writer, u >> k, width);
    }
    for (size_t i = 0; i < count; ++i) writer.put(fold_signed(input[i]), k);
    return k;
}
inline unsigned read_rice_block(BitReader& reader, int32_t* output, size_t count, unsigned width = 16) {
    validate_width(width);
    const unsigned k = reader.get(5);
    if (k >= width) throw std::runtime_error("Invalid AAL Rice parameter");
    std::vector<uint32_t> quotients(count);
    for (size_t i = 0; i < count; ++i) quotients[i] = read_quotient(reader, width);
    for (size_t i = 0; i < count; ++i) {
        const uint64_t u = (uint64_t(quotients[i]) << k) | reader.get(k);
        if (u >= (uint64_t(1) << width)) throw std::runtime_error("AAL residual exceeds sample width");
        output[i] = unfold_signed(uint32_t(u));
    }
    return k;
}

struct EnhancementPacket {
    std::vector<uint8_t> bytes;
    uint8_t padding_bytes = 0;
    size_t meaningful_bytes = 0;
    bool raw = false;
};
inline EnhancementPacket finish_enhancement(BitWriter& writer, bool raw = false) {
    writer.put(0, 2); // Syntax end marker, not an optional extra zero byte.
    writer.align_byte();
    EnhancementPacket result;
    result.bytes = writer.data();
    result.meaningful_bytes = result.bytes.size();
    result.padding_bytes = uint8_t((-result.meaningful_bytes) & 7);
    result.bytes.resize(result.meaningful_bytes + result.padding_bytes, 0);
    result.raw = raw;
    return result;
}
inline EnhancementPacket encode_raw_enhancement(const int32_t* left, const int32_t* right, unsigned width = 16,
                                                size_t samples = 1024) {
    validate_width(width);
    if(!left||!right||(samples!=1024&&samples!=2048))throw std::invalid_argument("Invalid AAL residual block size");
    BitWriter writer;
    writer.put(0b01001, 5);
    for (const int32_t* channel : {left, right})
        for (size_t i = 0; i < samples; ++i) writer.put(uint32_t(channel[i]), width);
    return finish_enhancement(writer, true);
}
inline EnhancementPacket encode_no_lpc(const int32_t* left, const int32_t* right, unsigned width = 16,
                                      bool allow_raw = true, size_t samples = 1024) {
    validate_width(width);
    if(!left||!right||(samples!=1024&&samples!=2048))throw std::invalid_argument("Invalid AAL residual block size");
    const size_t half=samples/2;
    BitWriter writer;
    writer.put(0b0100000, 7); // 1 reserved + 2 channel count + 2 raw mode + 2 decorrelation.
    for (const int32_t* channel : {left, right}) {
        writer.put(0, 4); // No temporal prediction.
        writer.put(1, 2); // Two equally sized 512-sample partitions.
        write_rice_block(writer, channel, half, width);
        write_rice_block(writer, channel + half, half, width);
    }
    auto result = finish_enhancement(writer);
    if (allow_raw) {
        auto raw = encode_raw_enhancement(left, right, width, samples);
        // Original formatter keeps the coded form only while its meaningful
        // byte length is at most 95 percent of the uncompressed PCM bytes.
        if (double(result.meaningful_bytes) > double(samples * 2 * width / 8) * 0.95) return raw;
    }
    return result;
}
template<size_t Samples>
inline std::array<std::array<int32_t, Samples>, 2> decode_no_lpc_samples(const EnhancementPacket& packet, unsigned width = 16) {
    static_assert(Samples==1024||Samples==2048,"Invalid AAL residual block size");
    validate_width(width);
    if (packet.padding_bytes > packet.bytes.size()) throw std::runtime_error("Invalid AAL byte padding");
    BitReader reader(packet.bytes.data(), packet.bytes.size() - packet.padding_bytes);
    if (reader.get(1) || reader.get(2) != 2) throw std::runtime_error("Unsupported AAL channel header");
    const unsigned mode = reader.get(2);
    std::array<std::array<int32_t, Samples>, 2> result{};
    if (mode == 1) {
        const uint32_t sign = uint32_t(1) << (width - 1);
        for (auto& channel : result) for (auto& x : channel) {
            const uint32_t u = reader.get(width);
            x = (u & sign) ? int32_t(int64_t(u) - (int64_t(1) << width)) : int32_t(u);
        }
    } else if (mode == 0) {
        if (reader.get(2) != 0) throw std::runtime_error("Unsupported AAL decorrelation");
        for (auto& channel : result) {
            if (reader.get(4) != 0) throw std::runtime_error("Unsupported AAL prediction mode");
            const unsigned partitions = reader.get(2) + 1;
            if (Samples % partitions) throw std::runtime_error("Unsupported AAL partition size");
            const size_t count = Samples / partitions;
            for (unsigned j = 0; j < partitions; ++j) read_rice_block(reader, channel.data() + count * j, count, width);
        }
    } else throw std::runtime_error("Unsupported AAL enhancement mode");
    if (reader.get(2)) throw std::runtime_error("Nonzero AAL end marker");
    reader.align_byte();
    if (reader.bit_count() / 8 != packet.bytes.size() - packet.padding_bytes)
        throw std::runtime_error("Extra meaningful bytes after AAL end marker");
    return result;
}
inline std::array<std::array<int32_t, 1024>, 2> decode_no_lpc(const EnhancementPacket& packet, unsigned width = 16) {
    return decode_no_lpc_samples<1024>(packet,width);
}

}} // namespace independent_aal::compression
